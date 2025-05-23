// Copyright 2009-2025 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2025, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// of the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

// Copyright 2015 IBM Corporation

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//   http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <sst_config.h>

#include <stdlib.h>
#include <sstream>
#include <algorithm>
#include <assert.h>
#include <cmath>
#include <regex>

#include "c_AddressHasher.hpp"

using namespace std;
using namespace SST;
using namespace SST::CramSim;



c_AddressHasher::c_AddressHasher(ComponentId_t id, Params &params, Output* out, unsigned channels, unsigned ranks, unsigned bankGroups,
        unsigned banks, unsigned rows, unsigned cols, unsigned pChannels) : SubComponent(id), output(out), k_pNumChannels(channels),
k_pNumRanks(ranks), k_pNumBankGroups(bankGroups), k_pNumBanks(banks), k_pNumRows(rows), k_pNumCols(cols), k_pNumPseudoChannels(pChannels) {
    build(params);
}

void c_AddressHasher::build(Params &params) {
  // read params here
  bool l_found = false;
  k_addressMapStr = (string)params.find<string>("strAddressMapStr", "_r_l_b_R_B_h_", l_found);
  output->output("Address map string: %s\n", k_addressMapStr.c_str());

  string l_mapCopy = k_addressMapStr;

  //first, remove all _'s
  l_mapCopy.erase(remove(l_mapCopy.begin(), l_mapCopy.end(), '_'), l_mapCopy.end());
  // remove trailing newline
  l_mapCopy.erase(remove(l_mapCopy.begin(), l_mapCopy.end(), '\n'), l_mapCopy.end());



  // this is replacement code for the above, which is generally safer than my implemention
  // of parsePattern below
  uint32_t l_curPos = 0;
  vector<string> l_simpleOrder;
  pair<string,uint32_t> l_parsedData;

  while(!l_mapCopy.empty()) {
    parsePattern(&l_mapCopy, &l_parsedData);
      m_structureSizes[l_parsedData.first] += l_parsedData.second;
      for(int ii=0; ii < l_parsedData.second; ii++) {
    m_bitPositions[l_parsedData.first].push_back(l_curPos);
    l_curPos++;
      }
      l_simpleOrder.push_back(l_parsedData.first);

  } // while !l_mapCopy.empty()


  // pull config file sizes
  k_pBurstSize = (uint32_t)params.find<uint32_t>("numBytesPerTransaction", 1,l_found);
  assert(k_pNumPseudoChannels>0);

  // check for simple version address map
  bool l_allSimple = true;
  for(auto l_iter : m_bitPositions) {
    if(l_iter.second.size() > 1) {
      l_allSimple = false;
      break;
    }
  }

  if(l_allSimple) { // if simple address detected, reset the bitPositions structureSizes
    // reset bitPositions
    for(auto l_iter : l_simpleOrder) {
      m_bitPositions[l_iter].clear();
    }

    l_curPos = 0;
    map<string, uint32_t> l_cfgBits;
    l_cfgBits["C"] = (uint32_t)log2(k_pNumChannels);
    l_cfgBits["c"] = (uint32_t)log2(k_pNumPseudoChannels);   // set the number of address bits assigned to pseudo channel.
    l_cfgBits["R"] = (uint32_t)log2(k_pNumRanks);
    l_cfgBits["B"] = (uint32_t)log2(k_pNumBankGroups);
    l_cfgBits["b"] = (uint32_t)log2(k_pNumBanks);
    l_cfgBits["r"] = (uint32_t)log2(k_pNumRows);
    l_cfgBits["l"] = (uint32_t)log2(k_pNumCols);
    l_cfgBits["h"] = (uint32_t)log2(k_pBurstSize);

    for(auto l_iter : l_simpleOrder) {
      m_structureSizes[l_iter] = l_cfgBits[l_iter];
      for(int ii = 0; ii < l_cfgBits[l_iter]; ii++) {
    m_bitPositions[l_iter].push_back(l_curPos);
    l_curPos++;
      }
    }

  } // if(allSimple)

  //
  // now verify that the address map and other params make sense
  //

  // Channels
  auto l_it = m_structureSizes.find("C"); // channel
  if(l_it == m_structureSizes.end()) { // if not found
    if(k_pNumChannels > 1) {
      output->fatal(CALL_INFO, -1, "%s, Number of Channels (%u) is greater than 1, but no Channels were specified (C) in the address map! Aborting!\n",
              getName().c_str(), k_pNumChannels);
    }
  } else { // found in map
    auto l_aNumChannels = (1 << l_it->second);
    if(l_aNumChannels > k_pNumChannels) {
      output->output("%s, Warning!: Number of address map channels is larger than numChannelsPerDimm.\nSome channels will be unused\n", getName().c_str());
    }
    if(l_aNumChannels < k_pNumChannels) { // some addresses have nowhere to go
      output->fatal(CALL_INFO, -1, "%s, Error!: Number of address map channels is smaller than numChannelsPerDimm. Aborting!", getName().c_str());
    }
  } // else found in map

   //Pseudo channel
   l_it = m_structureSizes.find("c"); // Pseudo channel
  if(l_it == m_structureSizes.end()) { // if not found
  if(k_pNumPseudoChannels > 1) {

      output->fatal(CALL_INFO, -1, "%s, Number of Pseudo Channels (%u) is greater than 1, but no Channels were specified (c) in the address map! Aborting!\n",
              getName().c_str(), k_pNumPseudoChannels);
    }
  } else { // found in map
    auto l_aNumPChannels = (1 << l_it->second);
    if(l_aNumPChannels > k_pNumPseudoChannels) {
      output->output("%s, Warning!: Number of address map channels is larger than numPseudoChannels.\n"
              "Some channels will be unused\n\n", getName().c_str());
    }
    if(l_aNumPChannels < k_pNumPseudoChannels) { // some addresses have nowhere to go
      output->fatal(CALL_INFO, -1, "%s, Error!: Number of address map channels is smaller than numPseudoChannels. Aborting!\n", getName().c_str());
    }
  } // else found in map

  // Ranks
  l_it = m_structureSizes.find("R");
  if(l_it == m_structureSizes.end()) { // if not found
    if(k_pNumRanks > 1) {
      output->fatal(CALL_INFO, -1, "%s, Number of Ranks (%u) is greater than 1, but no Ranks were specified (R) en the address map! Aborting!\n",
              getName().c_str(), k_pNumRanks);
    }
  } else { // found in map
    auto l_aNumRanks = (1 << l_it->second);
    if(l_aNumRanks > k_pNumRanks) {
      output->output("%s, Warning!: Number of address map Ranks is larger than numRanksPerChannel.\n"
              "Some Ranks will be unused\n\n", getName().c_str());
    }
    if(l_aNumRanks < k_pNumRanks) { // some addresses have nowhere to go
      output->fatal(CALL_INFO, -1, "%s, Error!: Number of address map Ranks is smaller than numRanksPerChannel. Aborting!\n", getName().c_str());
    }
  } // else found in map

  // BankGroups
  l_it = m_structureSizes.find("B");
  if(l_it == m_structureSizes.end()) { // if not found
    if(k_pNumBankGroups > 1) {
      output->fatal(CALL_INFO, -1, "%s, Number of BankGroups (%u) is greater than 1, but no BankGroups were specified (B) in the address map! Aborting!\n",
              getName().c_str(), k_pNumBankGroups);
    }
  } else { // found in map
    auto l_aNumBankGroups = (1 << l_it->second);
    if(l_aNumBankGroups > k_pNumBankGroups) {
      output->output("%s, Warning!: Number of address map bankGroups is larger than numBankGroupsPerRank.\n"
              "Some BankGroups will be unused\n\n", getName().c_str());
    }
    if(l_aNumBankGroups < k_pNumBankGroups) { // some addresses have nowhere to go
      output->fatal(CALL_INFO, -1, "%s, Error!: Number of address map bankGroups is smaller than numBankGroupsPerRank. Aborting!\n", getName().c_str());
    }
  } // else found in map

  // Banks
  l_it = m_structureSizes.find("b");
  if(l_it == m_structureSizes.end()) { // if not found
    if(k_pNumBanks > 1) {
      output->fatal(CALL_INFO, -1, "%s, Number of Banks (%u) is greater than 1, but no "
       "Banks were specified (b) in the address map! Aborting!\n", getName().c_str(), k_pNumBanks);
    }
  } else { // found in map
    auto l_aNumBanks = (1 << l_it->second);
    if(l_aNumBanks > k_pNumBanks) {
      output->output("%s, Warning!: Number of address map Banks is larger than numBanksPerBankGroup.\n"
            "Some Banks will be unused\n\n", getName().c_str());
    }
    if(l_aNumBanks < k_pNumBanks) { // some addresses have nowhere to go
      output->fatal(CALL_INFO, -1, "%s, Error!: Number of address map Banks is smaller than numBanksPerBankGroup. Aborting!\n", getName().c_str());
    }
  } // else found in map

  // Rows
  l_it = m_structureSizes.find("r");
  if(l_it == m_structureSizes.end()) { // if not found
    if(k_pNumRows > 1) {
      output->fatal(CALL_INFO, -1, "%s, Number of Rows (%u) is greater than 1, but no "
              "Rows were specified (r) in the address map! Aborting!\n", getName().c_str(), k_pNumRows);
    }
  } else { // found in map
    auto l_aNumRows = (1 << l_it->second);
    if(l_aNumRows > k_pNumRows) {
      output->output("%s, Warning!: Number of address map Rows is larger than numRowsPerBank.\n"
              "Some Rows will be unused\n\n", getName().c_str());
    }
    if(l_aNumRows < k_pNumRows) { // some addresses have nowhere to go
      output->fatal(CALL_INFO, -1, "%s, Error!: Number of address map Rows is smaller than numRowsPerBank. Aborting!\n", getName().c_str());
    }
  } // else found in map

  // Cols
  l_it = m_structureSizes.find("l");
  if(l_it == m_structureSizes.end()) { // if not found
    if(k_pNumCols > 1) {
      output->fatal(CALL_INFO, -1, "%s, Number of Cols (%u) is greater than 1, but no "
              "Cols were specified (l) [el] in the address map! Aborting!\n", getName().c_str(), k_pNumCols);
    }
  } else { // found in map
    auto l_aNumCols = (1 << l_it->second);
    if(l_aNumCols > k_pNumCols) {
      output->output("%s, Warning!: Number of address map Cols is larger than numColsPerBank.\n"
              "Some Cols will be unused\n\n", getName().c_str());
    }
    if(l_aNumCols < k_pNumCols) { // some addresses have nowhere to go
      output->fatal(CALL_INFO, -1, "%s, Error!: Number of address map Cols is smaller than numColsPerBank. Aborting!\n", getName().c_str());
    }
  } // else found in map

  // Cacheline/Burst size
  l_it = m_structureSizes.find("h");
  if(l_it == m_structureSizes.end()) { // if not found
    if(k_pBurstSize > 1) {
      output->fatal(CALL_INFO, -1, "%s, Burst size (%u) is greater than 1, but no "
       "Cachelines were specified (h) in the address map! Aborting!\n", getName().c_str(), k_pBurstSize);
    }
  } else { // found in map
    auto l_aNumCachelines = (1 << l_it->second);
    if(l_aNumCachelines != k_pBurstSize) {
      output->fatal(CALL_INFO, -1, "%s, Error!: Number of address map Cachelines is not equal to numBytesPerTransaction.\n"
              "Make sure that the address map cachelines (h) are equal to numBytesPerTransaction (i.e. 2**h == numBytesPerTransaction!", getName().c_str());
    }
  } // else found in map

} // c_AddressHasher(SST::Params)


void c_AddressHasher::fillHashedAddress(c_HashedAddress *x_hashAddr, const ulong x_address) {
  ulong l_cur=0;
  ulong l_cnt=0;

  //channel
  auto l_bitPos = m_bitPositions.find("C");
  if(l_bitPos == m_bitPositions.end()) { // not found
    x_hashAddr->setChannel(0);
  } else {
    l_cur=0;
    for(l_cnt=0;l_cnt<l_bitPos->second.size();l_cnt++) {
      ulong l_val = l_bitPos->second[l_cnt];
      ulong l_tmp = (((ulong)1 << l_val) & x_address) >> (l_val - l_cnt);

      l_cur |= l_tmp;
    }
    x_hashAddr->setChannel(l_cur);
  }

  //pseudo channel
   l_bitPos = m_bitPositions.find("c");
  if(l_bitPos == m_bitPositions.end()) { // not found
    x_hashAddr->setPChannel(0);
  } else {
    l_cur=0;
    for(l_cnt=0;l_cnt<l_bitPos->second.size();l_cnt++) {
      ulong l_val = l_bitPos->second[l_cnt];
      ulong l_tmp = (((ulong)1 << l_val) & x_address) >> (l_val - l_cnt);

      l_cur |= l_tmp;
    }
    x_hashAddr->setPChannel(l_cur);
  }

  //rank
  l_bitPos = m_bitPositions.find("R");
  if(l_bitPos == m_bitPositions.end()) { // not found
    x_hashAddr->setRank(0);
  } else {
    l_cur=0;
    for(l_cnt=0;l_cnt<l_bitPos->second.size();l_cnt++) {
      ulong l_val = l_bitPos->second[l_cnt];
      ulong l_tmp = (((ulong)1 << l_val) & x_address) >> (l_val - l_cnt);

      l_cur |= l_tmp;
    }
    x_hashAddr->setRank(l_cur);
  }

  //bankGroup
  l_bitPos = m_bitPositions.find("B");
  if(l_bitPos == m_bitPositions.end()) { // not found
    x_hashAddr->setBankGroup(0);
  } else {
    l_cur=0;
    for(l_cnt=0;l_cnt<l_bitPos->second.size();l_cnt++) {
      ulong l_val = l_bitPos->second[l_cnt];
      ulong l_tmp = (((ulong)1 << l_val) & x_address) >> (l_val - l_cnt);

      l_cur |= l_tmp;
    }
    x_hashAddr->setBankGroup(l_cur);
  }

  //bank
  l_bitPos = m_bitPositions.find("b");
  if(l_bitPos == m_bitPositions.end()) { // not found
    x_hashAddr->setBank(0);
  } else {
    l_cur=0;
    for(l_cnt=0;l_cnt<l_bitPos->second.size();l_cnt++) {
      ulong l_val = l_bitPos->second[l_cnt];
      ulong l_tmp = (((ulong)1 << l_val) & x_address) >> (l_val - l_cnt);

      l_cur |= l_tmp;
    }
    x_hashAddr->setBank(l_cur);
  }

  //row
  l_bitPos = m_bitPositions.find("r");
  if(l_bitPos == m_bitPositions.end()) { // not found
    x_hashAddr->setRow(0);
  } else {
    l_cur=0;
    for(l_cnt=0;l_cnt<l_bitPos->second.size();l_cnt++) {
      ulong l_val = l_bitPos->second[l_cnt];
      ulong l_tmp = (((ulong)1 << l_val) & x_address) >> (l_val - l_cnt);

      l_cur |= l_tmp;
    }
    x_hashAddr->setRow(l_cur);
  }

  //col
  l_bitPos = m_bitPositions.find("l");
  if(l_bitPos == m_bitPositions.end()) { // not found
    x_hashAddr->setCol(0);
  } else {
    l_cur=0;
    for(l_cnt=0;l_cnt<l_bitPos->second.size();l_cnt++) {
      ulong l_val = l_bitPos->second[l_cnt];
      ulong l_tmp = (((ulong)1 << l_val) & x_address) >> (l_val - l_cnt);

      l_cur |= l_tmp;
    }
    x_hashAddr->setCol(l_cur);
  }

  //cacheline
  l_bitPos = m_bitPositions.find("h");
  if(l_bitPos == m_bitPositions.end()) { // not found
    x_hashAddr->setCacheline(0);
  } else {
    l_cur=0;
    for(l_cnt=0;l_cnt<l_bitPos->second.size();l_cnt++) {
      ulong l_val = l_bitPos->second[l_cnt];
      ulong l_tmp = (((ulong)1 << l_val) & x_address) >> (l_val - l_cnt);

      l_cur |= l_tmp;
    }
    x_hashAddr->setCacheline(l_cur);
  }

  unsigned l_bankId =
    x_hashAddr->getBank()
    + x_hashAddr->getBankGroup() * k_pNumBanks
    + x_hashAddr->getRank()    * k_pNumBanks * k_pNumBankGroups
    + x_hashAddr->getPChannel()  *  k_pNumBanks * k_pNumBankGroups * k_pNumRanks
    + x_hashAddr->getChannel()   * k_pNumPseudoChannels * k_pNumBanks * k_pNumBankGroups * k_pNumRanks;


  unsigned l_rankId =
            x_hashAddr->getRank()
          + x_hashAddr->getPChannel()  * k_pNumRanks
          + x_hashAddr->getChannel()   * k_pNumPseudoChannels * k_pNumRanks;

  x_hashAddr->setBankId(l_bankId);
  x_hashAddr->setRankId(l_rankId);
   // output->output("0x%" PRIx64 "\t", x_address);  x_hashAddr->print();

} // fillHashedAddress(c_HashedAddress, x_address)

ulong c_AddressHasher::getAddressForBankId(const unsigned x_bankId) {
  // obtain the bank group rank and channel of this bankId;
  unsigned l_cur = x_bankId;
  unsigned l_chanSize = k_pNumBanks * k_pNumBankGroups * k_pNumRanks * k_pNumPseudoChannels;
  unsigned l_pchanSize = k_pNumBanks * k_pNumBankGroups * k_pNumRanks;
  unsigned l_rankSize = k_pNumBanks * k_pNumBankGroups;
  unsigned l_bankGroupSize = k_pNumBanks;

  unsigned l_chan=0,l_pchan=0,l_rank=0,l_bankgroup=0,l_bank=0;

  output->output("Getting an address for bankId %u\n", x_bankId);

  while(l_cur >= l_chanSize) {
    l_cur -= l_chanSize;
    l_chan++;
  }

  while(l_cur >= l_pchanSize) {
    l_cur -= l_pchanSize;
    l_pchan++;
  }

  while(l_cur >= l_rankSize) {
    l_cur -= l_rankSize;
    l_rank++;
  }

  while(l_cur >= l_bankGroupSize) {
    l_cur -= l_bankGroupSize;
    l_bankgroup++;
  }

  l_bank = l_cur;

  output->output("Final %u %u %u %u %u\n", l_chan, l_pchan, l_rank, l_bankgroup, l_bank);

  ulong l_address = 0;
  {
    ulong l_tmp = l_bank;
    ulong l_tOut = 0;
    unsigned l_curPos = 0;
    while(l_tmp) {
      l_tOut += (l_tmp & 0x1) << m_bitPositions["b"][l_curPos];
      l_tmp >>= 1;
      l_curPos++;
    }

    l_address += l_tOut;
  }

  {
    ulong l_tmp = l_bankgroup;
    ulong l_tOut = 0;
    unsigned l_curPos = 0;
    while(l_tmp) {
      l_tOut += (l_tmp & 0x1) << m_bitPositions["B"][l_curPos];
      l_tmp >>= 1;
      l_curPos++;
    }

    l_address += l_tOut;
  }

  {
    ulong l_tmp = l_rank;
    ulong l_tOut = 0;
    unsigned l_curPos = 0;
    while(l_tmp) {
      l_tOut += (l_tmp & 0x1) << m_bitPositions["R"][l_curPos];
      l_tmp >>= 1;
      l_curPos++;
    }

    l_address += l_tOut;
  }

  {
    ulong l_tmp = l_pchan;
    ulong l_tOut = 0;
    unsigned l_curPos = 0;
    while(l_tmp) {
      l_tOut += (l_tmp & 0x1) << m_bitPositions["c"][l_curPos];
      l_tmp >>= 1;
      l_curPos++;
    }

    l_address += l_tOut;
  }

  {
    ulong l_tmp = l_chan;
    ulong l_tOut = 0;
    unsigned l_curPos = 0;
    while(l_tmp) {
      l_tOut += (l_tmp & 0x1) << m_bitPositions["C"][l_curPos];
      l_tmp >>= 1;
      l_curPos++;
    }

    l_address += l_tOut;
  }

 // output->output("Returning address 0x%" PRIx64 "\n", l_address);

  return(l_address);
} // getAddressForBankId(const unsigned x_bankId)

// parsePattern
// takes in complete or partially parsed address map string
// returns a pair with the matched string and the size of the field
// also removes the matched portion of the pattern from l_inStr
//
// everything is parsed from the end of the string backwards
void c_AddressHasher::parsePattern(string *x_inStr, std::pair<string,uint32_t> *x_outPair) {
  assert(x_inStr != nullptr);
  assert(x_outPair != nullptr);

  if(x_inStr->empty()) {
    x_outPair->first = "";
    return;
  }

  string l_sizeStr("");

  bool l_matched=false;
  bool l_sizeMatched = false;

  auto l_sIter = x_inStr->rbegin();
  while(!l_matched) {
    if(isdigit(*l_sIter)) {
      if(l_sizeMatched) {
    output->fatal(CALL_INFO, -1, "%s, Weird parsing detected!\nParsing error at %c in address map string %s]\n",
                getName().c_str(), (*l_sIter), (*x_inStr).c_str());
      }
      l_sizeStr = *l_sIter + l_sizeStr;
    } else if(isalpha(*l_sIter)) {
      if(!(*l_sIter == 'r' || *l_sIter == 'l' || *l_sIter == 'R' || *l_sIter == 'B' ||
       *l_sIter == 'b' || *l_sIter == 'C' || *l_sIter == 'h' ||*l_sIter == 'c' ||*l_sIter == 'x')) {
          output->fatal(CALL_INFO, -1, "%s, Parsing error at %c in address map string %s\n",
                  getName().c_str(), (*l_sIter), (*x_inStr).c_str());
      }

      x_outPair->first = *l_sIter;
      if(l_sizeMatched) {
    x_outPair->second = stoi(l_sizeStr);
      } else {
    x_outPair->second = 1;
      }

      x_inStr->erase(next(l_sIter).base(),x_inStr->end()); // remove the matched portion
      //output->output("Returning %s %d\n", x_outPair->first.c_str(), x_outPair->second);
      break;
    } else if(*l_sIter == ':') {
      l_sizeMatched = true;
    } else {
      output->fatal(CALL_INFO, -1, "%s, Parsing error at %c in address map string %s\n", getName().c_str(), (*l_sIter), (*x_inStr).c_str());
    }
    l_sIter++;
    if(l_sIter == x_inStr->rend()) {
      break;
    }
  } // while(!l_matched)
} // parsePattern(string, pair<string,uint32_t>)
