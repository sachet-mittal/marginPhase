/*
 * Copyright (C) 2009-2018 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#ifndef REALIGNER_H_
#define REALIGNER_H_

#include "sonLib.h"
#include "pairwiseAligner.h"
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <ctype.h>

/*
 * Parameter object for polish algorithm
 */

typedef struct _repeatSubMatrix RepeatSubMatrix; 

typedef struct _polishParams {
	bool useRunLengthEncoding;
	double referenceBasePenalty; // used by poa_getConsensus to weight against picking the reference base
	double minPosteriorProbForAlignmentAnchor; // used by by poa_getAnchorAlignments to determine which alignment pairs
	// to use for alignment anchors during poa_realignIterative
	Hmm *hmm; // Pair hmm used for aligning reads to the reference.
	StateMachine *sM; // Statemachine derived from the hmm
	PairwiseAlignmentParameters *p; // Parameters object used for aligning
	RepeatSubMatrix *repeatSubMatrix; // Repeat submatrix	
} PolishParams;

PolishParams *polishParams_readParams(FILE *fileHandle);

void polishParams_printParameters(PolishParams *polishParams, FILE *fh);

void polishParams_destruct(PolishParams *polishParams);

/*
 * Basic data structures for representing a POA alignment.
 */
 
typedef struct _Poa {
	char *refString; // The reference string
	stList *nodes; 
} Poa;

typedef struct _poaNode {
	stList *inserts; // Inserts that happen immediately after this position
	stList *deletes; // Deletes that happen immediately after this position
	char base; // Char representing base, e.g. 'A', 'C', etc.
	double *baseWeights; // Array of length SYMBOL_NUMBER, encoding the weight given go each base, using the Symbol enum
	stList *observations; // Individual events representing event, a list of PoaObservations
} PoaNode;

typedef struct _poaInsert {
	char *insert; // String representing characters of insert e.g. "GAT", etc.
	double weight;
} PoaInsert;

typedef struct _poaDelete {
	int64_t length; // Length of delete
	double weight;
} PoaDelete;

typedef struct _poaBaseObservation {
	int64_t readNo;
	int64_t offset;
	double weight;
} PoaBaseObservation;

/*
 * Poa functions.
 */

/*
 * Creates a POA representing the given reference sequence, with one node for each reference base and a 
 * prefix 'N' base to represent place to add inserts/deletes that precede the first position of the reference.
 */
Poa *poa_getReferenceGraph(char *reference);

/*
 * Adds to given POA the matches, inserts and deletes from the alignment of the given read to the reference.
 * Adds the inserts and deletes so that they are left aligned.
 */
void poa_augment(Poa *poa, char *read, int64_t readNo, stList *matches, stList *inserts, stList *deletes);

/*
 * Creates a POA representing the reference and the expected inserts / deletes and substitutions from the 
 * alignment of the given set of reads aligned to the reference. Anchor alignments is a set of pairwise 
 * alignments between the reads and the reference sequence. There is one alignment for each read. See 
 * poa_getAnchorAlignments. The anchorAlignments can be null, in which case no anchors are used.
 */
Poa *poa_realign(stList *reads, stList *anchorAlignments, char *reference,
			  	 PolishParams *polishParams);

/*
 * Generates a set of anchor alignments for the reads aligned to a consensus sequence derived from the poa.
 * These anchors can be used to restrict subsequent alignments to the consensus to generate a new poa.
 * PoaToConsensusMap is a map from the positions in the poa reference sequence to the derived consensus 
 * sequence. See poa_getConsensus for description of poaToConsensusMap. If poaToConsensusMap is NULL then
 * the alignment is just the reference sequence of the poa.
 */
stList *poa_getAnchorAlignments(Poa *poa, int64_t *poaToConsensusMap, int64_t noOfReads, 
							    PolishParams *polishParams);
							    
/*
 * Generates a set of maximal expected alignments for the reads aligned to the the POA reference sequence.
 * Unlike the draft anchor alignments, these are designed to be complete, high quality alignments.
 */
stList *poa_getReadAlignmentsToConsensus(Poa *poa, stList *reads, PolishParams *polishParams);

/*
 * Prints representation of the POA.
 */
void poa_print(Poa *poa, FILE *fH, float indelSignificanceThreshold);

/*
 * Prints some summary stats on the POA.
 */
void poa_printSummaryStats(Poa *poa, FILE *fH);

/*
 * Creates a consensus reference sequence from the POA. poaToConsensusMap is a pointer to an 
 * array of integers of length str(poa->refString), giving the index of the reference positions 
 * alignment to the consensus sequence, or -1 if not aligned. It is initialised as a 
 * return value of the function.
 */
char *poa_getConsensus(Poa *poa, int64_t **poaToConsensusMap, PolishParams *polishParams);

/*
 * Iteratively used poa_realign and poa_getConsensus to refine the median reference sequence 
 * for the given reads and the starting reference.
 */
Poa *poa_realignIterative(stList *reads, stList *anchorAlignments, char *reference, PolishParams *polishParams);

/*
 * Greedily evaluate the top scoring indels.  
 */
Poa *poa_checkMajorIndelEditsGreedily(Poa *poa, stList *reads, PolishParams *polishParams);
				 
void poa_destruct(Poa *poa);

/*
 * Finds shift, expressed as a reference coordinate, that the given substring str can
 * be shifted left in the refString, starting from a match at refStart.
 */
int64_t getShift(char *refString, int64_t refStart, char *str, int64_t length);

/*
 * Get sum of weights for reference bases in poa - proxy to agreement of reads
 * with reference.
 */
double poa_getReferenceNodeTotalMatchWeight(Poa *poa);

/*
 * Get sum of weights for delete in poa - proxy to delete disagreement of reads
 * with reference.
 */
double poa_getDeleteTotalWeight(Poa *poa);

/*
 * Get sum of weights for inserts in poa - proxy to insert disagreement of reads
 * with reference.
 */
double poa_getInsertTotalWeight(Poa *poa);

/*
 * Get sum of weights for non-reference bases in poa - proxy to disagreement of read positions
 * aligned with reference.
 */
double poa_getReferenceNodeTotalDisagreementWeight(Poa *poa);

/*
 * Functions for run-length encoding/decoding with POAs
 */

// Data structure for representing RLE strings
typedef struct _rleString {
	char *rleString; //Run-length-encoded (RLE) string
	int64_t *repeatCounts; // Count of repeat for each position in rleString
	int64_t *rleToNonRleCoordinateMap; // For each position in the RLE string the corresponding, left-most position
	// in the expanded non-RLE string
	int64_t *nonRleToRleCoordinateMap; // For each position in the expanded non-RLE string the corresponding the position
	// in the RLE string
	int64_t length; // Length of the rleString
	int64_t nonRleLength; // Length of the expanded non-rle string
} RleString;

RleString *rleString_construct(char *string);

void rleString_destruct(RleString *rlString);

// Data structure for storing log-probabilities of observing
// one repeat count given another
struct _repeatSubMatrix {
	double *logProbabilities;
	int64_t maximumRepeatLength;
};

/*
 * Reads the repeat count matrix from a given input file.
 */
 
RepeatSubMatrix *repeatSubMatrix_constructEmpty();

void repeatSubMatrix_destruct(RepeatSubMatrix *repeatSubMatrix);



/*
 * Gets the log probability of observing a given repeat conditioned on an underlying repeat count and base.
 */
double repeatSubMatrix_getLogProb(RepeatSubMatrix *repeatSubMatrix, Symbol base, 
								  int64_t observedRepeatCount, int64_t underlyingRepeatCount);

/*
 * As gets, but returns the address.
 */
double *repeatSubMatrix_setLogProb(RepeatSubMatrix *repeatSubMatrix, Symbol base, int64_t observedRepeatCount, int64_t underlyingRepeatCount);

/*
 * Gets the log probability of observing a given set of repeat observations conditioned on an underlying repeat count and base.
 */
double repeatSubMatrix_getLogProbForGivenRepeatCount(RepeatSubMatrix *repeatSubMatrix, Symbol base, stList *observations,
												     stList *rleReads, int64_t underlyingRepeatCount);

/*
 * Gets the maximum likelihood underlying repeat count for a given set of observed read repeat counts.
 * Puts the ml log probility in *logProbabilty.
 */
int64_t repeatSubMatrix_getMLRepeatCount(RepeatSubMatrix *repeatSubMatrix, Symbol base, 
										 stList *observations, stList *rleReads, double *logProbability);

/*
 * Takes a POA done in run-length space and returns an expanded consensus string in
 * non-run-length space.
 */
char *expandRLEConsensus(Poa *poa, stList *rlReads, RepeatSubMatrix *repeatSubMatrix);

/*
 * Translate a sequence of aligned pairs (as stIntTuples) whose coordinates are monotonically increasing 
 * in both underlying sequences (seqX and seqY) into an equivalent run-length encoded space alignment.
 */
stList *runLengthEncodeAlignment(stList *alignment, RleString *seqX, RleString *seqY);

/*
 * Make edited string with given insert. Edit start is the index of the position to insert the string.
 */
char *addInsert(char *string, char *insertString, int64_t editStart);

/*
 * Make edited string with given insert. Edit start is the index of the first position to delete from the string.
 */
char *removeDelete(char *string, int64_t deleteLength, int64_t editStart);


/*
 * Generates MEA alignments between two string. Anchor alignment may be null.
 * TODO: Move to cpecan
 */
stList *getPairwiseMEAAlignment(char *stringX, char *stringY, stList *anchorAlignment,
								PairwiseAlignmentParameters  *p, StateMachine *sM);


/*
 * Functions for processing BAMs
 */
 
// TODO: MOVE BAMCHUNKER TO PARSER .c
 
typedef struct _bamChunker {
    // file locations
	char *bamFile;
    // configuration
    uint64_t chunkSize;
    uint64_t chunkBoundary;
    bool includeSoftClip;
    // internal data
    stList *chunks;
    uint64_t chunkCount;
    int64_t itorIdx;
} BamChunker;

typedef struct _bamChunk {
	char *refSeqName;          // name of contig
    int64_t chunkBoundaryStart;  // the first 'position' where we have an aligned read
    int64_t chunkStart;        // the actual boundary of the chunk, calculations from chunkMarginStart to chunkStart
                               //  should be used to initialize the probabilities at chunkStart
    int64_t chunkEnd;          // same for chunk end
    int64_t chunkBoundaryEnd;    // no reads should start after this position
    BamChunker *parent;        // reference to parent (may not be needed)
} BamChunk;

BamChunker *bamChunker_construct(char *bamFile);
BamChunker *bamChunker_construct2(char *bamFile, uint64_t chunkSize, uint64_t chunkBoundary, bool includeSoftClip);

void bamChunker_destruct(BamChunker *bamChunker);

BamChunk *bamChunker_getNext(BamChunker *bamChunker);

BamChunk *bamChunk_construct();
BamChunk *bamChunk_construct2(char *refSeqName, int64_t chunkBoundaryStart, int64_t chunkStart, int64_t chunkEnd,
                              int64_t chunkBoundaryEnd, BamChunker *parent);

void bamChunk_destruct(BamChunk *bamChunk);

/*
 * Converts chunk of aligned reads into list of reads and alignments.
 */
uint32_t convertToReadsAndAlignments(BamChunk *bamChunk, stList *reads, stList *alignments);

#endif /* REALIGNER_H_ */