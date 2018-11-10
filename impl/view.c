/*
 * Copyright (C) 2018 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include "stPolish.h"
#include  "stView.h"
#include <time.h>
#include <htslib/sam.h>
#include <stRPHmm.h>

static int64_t *msaView_set(MsaView *view, int64_t refCoordinate, int64_t seqIndex) {
	return &(view->seqCoordinates[(view->refLength+1) * seqIndex + refCoordinate]);
}

int64_t msaView_getSeqCoordinate(MsaView *view, int64_t refCoordinate, int64_t seqIndex) {
	int64_t i = msaView_set(view, refCoordinate, seqIndex)[0];
	return i < 0 ? -1 : i-2;
}

int64_t msaView_getPrecedingInsertLength(MsaView *view, int64_t rightRefCoordinate, int64_t seqIndex) {
	int64_t i = msaView_set(view, rightRefCoordinate, seqIndex)[0];
	if(i < 0) {
		return 0;
	}
	if(rightRefCoordinate == 0) {
		return i-2;
	}
	int64_t j = msaView_set(view, rightRefCoordinate-1, seqIndex)[0];
	if(j < 0) {
		return i + j - 1;
	}
	return i - j - 1;
}

int64_t msaView_getPrecedingInsertStart(MsaView *view, int64_t rightRefCoordinate, int64_t seqIndex) {
	int64_t indelLength = msaView_getPrecedingInsertLength(view, rightRefCoordinate, seqIndex);
	if(indelLength == 0) {
		return -1;
	}
	return msaView_getSeqCoordinate(view, rightRefCoordinate, seqIndex) - indelLength;
}

int64_t msaView_getMaxPrecedingInsertLength(MsaView *view, int64_t rightRefCoordinate) {
	return view->maxPrecedingInsertLengths[rightRefCoordinate];
}

MsaView *msaView_construct(char *refSeq, char *refName,
		stList *refToSeqAlignments, stList *seqs, stList *seqNames) {
	MsaView *view = st_malloc(sizeof(MsaView));

	view->refSeq = refSeq; // This is not copied
	view->refLength = strlen(refSeq);
	view->refSeqName = refName; // This is not copied
	view->seqNo = stList_length(refToSeqAlignments);
	view->seqs = seqs; // This is not copied
	view->seqNames = seqNames; // Ditto
	view->seqCoordinates = st_calloc(view->seqNo * (view->refLength+1), sizeof(int64_t)); // At each
	// reference position for each non-ref sequence stores the coordinate of the position + 1 in the non-ref sequence aligned
	// to the reference position, if non-ref sequence is aligned at that position stores then stores -1 times the index
	// of the rightmost position aligned to the prefix of the reference up to that position + 1. The plus ones are to avoid
	// dealing with difference between 0 and -0. This storage format is sufficient to represent the alignment in an easy
	// to access format

	for(int64_t i=0; i<view->seqNo; i++) {
		stList *alignment = stList_get(refToSeqAlignments, i);
		for(int64_t j=0; j<stList_length(alignment); j++) {
			stIntTuple *alignedPair = stList_get(alignment, j);
			msaView_set(view, stIntTuple_get(alignedPair, 1), i)[0] = stIntTuple_get(alignedPair, 2)+2;
		}
		msaView_set(view, view->refLength, i)[0] = strlen(stList_get(view->seqs, i)) + 2;
		int64_t k = 1;
		for(int64_t j=0; j<view->refLength; j++) {
			int64_t *l = msaView_set(view, j, i);
			if(l[0] == 0) {
				l[0] = -k;
			}
			else {
				k = l[0];
			}
		}
	}

	view->maxPrecedingInsertLengths = st_calloc(view->refLength+1, sizeof(int64_t));
	for(int64_t j=0; j<view->refLength+1; j++) {
		int64_t maxIndelLength=0;
		for(int64_t i=0; i<view->seqNo; i++) {
			int64_t k=msaView_getPrecedingInsertLength(view, j, i);
			if(k > maxIndelLength) {
				maxIndelLength = k;
			}
		}
		view->maxPrecedingInsertLengths[j] = maxIndelLength;
	}

	return view;
}

void msaView_destruct(MsaView * view) {
	free(view->maxPrecedingInsertLengths);
	free(view->seqCoordinates);
	free(view);
}

static void printRepeatChar(FILE *fh, char repeatChar, int64_t repeatCount) {
	for(int64_t i=0; i<repeatCount; i++) {
		fprintf(fh, "%c", repeatChar);
	}
}

static void printSeqName(FILE *fh, char *seqName) {
	int64_t j = strlen(seqName);
	for(int64_t i=0; i<10; i++) {
		fprintf(fh, "%c", i < j ? seqName[i] : ' ');
	}
	fprintf(fh, " ");
}

static void msaView_print2(MsaView *view, int64_t refStart, int64_t length, FILE *fh) {
	// Print the reference
	printSeqName(fh, view->refSeqName == NULL ? "REF" : view->refSeqName);
	for(int64_t i=refStart; i<refStart+length; i++) {
		printRepeatChar(fh, '-', msaView_getMaxPrecedingInsertLength(view, i));
		fprintf(fh, "%c", view->refSeq[i]);
	}
	fprintf(fh, "\n");

	// Print the reads
	for(int64_t j=0; j<view->seqNo; j++) {
		if(view->seqNames == NULL) {
			char *seqName = stString_print("SEQ:%i", j);
			printSeqName(fh, seqName);
			free(seqName);
		}
		else {
			printSeqName(fh, stList_get(view->seqNames, j));
		}
		char *sequence = stList_get(view->seqs, j);
		for(int64_t i=refStart; i<refStart+length; i++) {
			int64_t indelLength = msaView_getPrecedingInsertLength(view, i, j);
			if(indelLength > 0) {
				int64_t indelStart = msaView_getPrecedingInsertStart(view, i, j);
				for(int64_t k=0; k<indelLength; k++) {
					fprintf(fh, "%c", sequence[indelStart+k]);
				}
			}
			printRepeatChar(fh, '-', msaView_getMaxPrecedingInsertLength(view, i) - indelLength);

			int64_t seqCoordinate = msaView_getSeqCoordinate(view, i, j);
			if(seqCoordinate != -1) {
				fprintf(fh, "%c", view->refSeq[i] == sequence[seqCoordinate] ? '*' : sequence[seqCoordinate]);
			}
			else {
				fprintf(fh, "+");
			}
		}
		fprintf(fh, "\n");
	}
	fprintf(fh, "\n");
}

void msaView_print(MsaView *view, FILE *fh) {
	int64_t width = 40;
	for(int64_t i=0; i<view->refLength; i+=width) {
		msaView_print2(view, i, (i+width < view->refLength) ? width : view->refLength-i, fh);
	}
}