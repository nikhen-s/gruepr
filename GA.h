#ifndef GA_H
#define GA_H

// Code related to the Genetic Algorithm used in gruepr

#include <random>

const int maxRecords = 300;                             // maximum number of records to optimally partition (this might be changable, but algortihm gets pretty slow with >300 records)
const int populationSize = 30000;						// the number of genomes in each generation--larger size is slower, but arguably more optimized result. A size of 5000 works with the default stack size. For size of 20000, stack size was increased to 16 MB. For 30000, increased to 32 MB.
const int tournamentSize = populationSize/500;          // most of the next generation is created by mating many pairs of parent genomes, each time chosen from genomes in a randomly selected tournament in the genepool
const int numGenerationsOfAncestors = 3;                // how many generations of ancestors to look back when preventing the selection of related mates:
                                                        //      1 = prevent if either parent is same (no siblings mating);
                                                        //      2 = prevent if any parent or grandparent is same (no siblings or 1st cousins);
                                                        //      3 = prevent if any parent, grandparent, or greatgrandparent is same (no siblings, 1st or 2nd cousins); etc.
const int topGenomeLikelihood = 33;                     // percent likelihood of selecting the best genome in the tournament as parent; if top is not selected, move to next best genome with same probability, and so on
const int numElites = 3;                    			// from each generation, this many highest scoring genomes are directly cloned into the next generation. Some suggest elitism helps speed genetic algorithms, but can lead to premature convergence. Having at least 1 elite significantly stabilizes the high score to end optimization
const int minGenerations = 40;                          // will keep optimizing for at least minGenerations
const int maxGenerations = 500;                         // will keep optimizing for at most maxGenerations
const int generationsOfStability = 25;                  // after minGenerations, if score has not improved for generationsOfStability, stop optimizing
const int minScoreStability = 100;                      // will keep optimizing until scoreStability (current score divided by range of scores within generationsOfStability) exceeds this
const int mutationLikelihood = 50;                      // percent likelihood of a mutation (when mutation occurs, another chance at mutation is given with same likelihood (iteratively))


namespace GA
{
    void tournamentSelectParents(int *const *const genePool, const int *const orderedIndex, int *const *const ancestors, int *&mom, int *&dad, int parentage[], std::mt19937 &pRNG);
    void mate(const int *const mom, const int *const dad, const int teamSize[], const int numTeams, int child[], const int genomeSize, std::mt19937 &pRNG);
    void mutate(int genome[], const int genomeSize, std::mt19937 &pRNG);
};


#endif // GA_H
