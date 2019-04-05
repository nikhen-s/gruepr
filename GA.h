#ifndef GA_H
#define GA_H

// Code related to the Genetic Algorithm used in gruepr

#include <cstdlib>
#include <algorithm>

const int maxRecords = 200;                             // maximum number of records to optimally partition (this might be changable, but algortihm gets pretty slow with >100 records)
const int populationSize = 30000;						// the number of genomes in each generation--larger size is slower, but arguably more optimized result. A size of 5000 works with the default stack size. For size of 20000, stack size was increased to 16 MB. For 30000, increased to 32 MB.
const int tournamentSize = populationSize/500;          // most of the next generation is created by mating many pairs of parent genomes, each time chosen from genomes in a randomly selected tournament in the genepool
const int topGenomeLikelihood = 20 * (RAND_MAX/100);	// first number gives probability out of 100 for selecting the best genome in the tournament as first parent to mate, then the best among the rest for second parent; if top is not selected, move to next best genome
const int numElites = 1;                    			// from each generation, this many highest scoring genomes are directly cloned into the next generation. Some suggest elitism helps speed genetic algorithms, but can lead to premature convergence. Having at least 1 elite significantly stabilizes the high score to end optimization
const int minGenerations = 40;                          // will keep optimizing for at least minGenerations
const int maxGenerations = 500;                         // will keep optimizing for at most maxGenerations
const int generationsOfStability = 25;                  // after minGenerations, if score has not improved for generationsOfStability, stop optimizing
const int mutationLikelihood = 50 * (RAND_MAX/100);     // first number gives probability out of 100 for a mutation (when mutation occurs, another chance at mutation is given with same likelihood (iteratively))


//struct used for tournament selection of parents to mate in genetic algorithm
struct tourneyPlayer
{
    int genome[maxRecords];
    double score;
};


namespace GA {
    void mate(int mom[], int dad[], int teamSize[], int numTeams, int child[], int genomeSize);
    void mutate(int genome[], int genomeSize);
};


#endif // GA_H
