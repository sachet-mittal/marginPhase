/*
 * Copyright (C) 2017 by Benedict Paten (benedictpaten@gmail.com) & Arthur Rand (arand@soe.ucsc.edu)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include "stRPHmm.h"


void writeSplitSams(char *bamInFile, char *bamOutBase,
                          stSet *haplotype1Ids, stSet *haplotype2Ids) {
    // prep
    char haplotype1BamOutFile[strlen(bamOutBase) + 7];
    strcpy(haplotype1BamOutFile, bamOutBase);
    strcat(haplotype1BamOutFile, ".1.sam");
    char haplotype2BamOutFile[strlen(bamOutBase) + 7];
    strcpy(haplotype2BamOutFile, bamOutBase);
    strcat(haplotype2BamOutFile, ".2.sam");
    char unmatchedBamOutFile[strlen(bamOutBase) + 15];
    strcpy(unmatchedBamOutFile, bamOutBase);
    strcat(unmatchedBamOutFile, ".unmatched.sam");

    // file management
    samFile *in = hts_open(bamInFile, "r");
    if (in == NULL) {
        st_errAbort("ERROR: Cannot open bam file %s\n", bamInFile);
    }
    bam_hdr_t *bamHdr = sam_hdr_read(in);
    bam1_t *aln = bam_init1();

    int r;
    st_logDebug("Writing haplotype output to: %s and %s \n", haplotype1BamOutFile, haplotype2BamOutFile);
    samFile *out1 = hts_open(haplotype1BamOutFile, "w");
    r = sam_hdr_write(out1, bamHdr);

    samFile *out2 = hts_open(haplotype2BamOutFile, "w");
    r = sam_hdr_write(out2, bamHdr);

    samFile *out3 = hts_open(unmatchedBamOutFile, "w");
    r = sam_hdr_write(out3, bamHdr);

    // read in input file, write out each read to one sam file
    int32_t readCountH1 = 0;
    int32_t readCountH2 = 0;
    int32_t readCountNeither = 0;
    while(sam_read1(in,bamHdr,aln) > 0) {

        char *readName = bam_get_qname(aln);
        if (stSet_search(haplotype1Ids, readName) != NULL) {
            r = sam_write1(out1, bamHdr, aln);
            readCountH1++;
        } else if (stSet_search(haplotype2Ids, readName) != NULL) {
            r = sam_write1(out2, bamHdr, aln);
            readCountH2++;
        } else {
            r = sam_write1(out3, bamHdr, aln);
            readCountNeither++;
        }
    }
    st_logDebug("Read counts:\n\thap1:%d\thap2:%d\tneither:%d\n", readCountH1, readCountH2, readCountNeither);

    bam_destroy1(aln);
    bam_hdr_destroy(bamHdr);
    sam_close(in);
    sam_close(out1);
    sam_close(out2);
    sam_close(out3);
}