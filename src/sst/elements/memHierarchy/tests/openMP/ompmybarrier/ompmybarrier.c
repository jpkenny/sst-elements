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

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

int main(int argc, char* argv[]) {

	int counter;
	const int n = 16;
        int id;

	#pragma omp parallel private(counter, id)
	{
                id = omp_get_thread_num();
		for(counter = 0; counter < n; counter++) {
			#pragma omp barrier

			printf("%d Performing iteration %d\n", id, counter);
			fflush(stdout);

			#pragma omp barrier
		}
	}

}
