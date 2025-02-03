/*
 * calculateStress.cpp
 *
 *  Created on: Nov 7, 2019
 *      Author: Nikhil
 */

#include <string>
#include <iostream>
#include <vector>
#include "neighbor_list.h"
#include "InteratomicForces.h"
#include "kim.h"
#include "BoxConfiguration.h"
#include "Configuration.h"
#include "SubConfiguration.h"
#include "Stress.h"
#include "typedef.h"
#include "StressTuple.h"
#include "helper.hpp"
#include "Rigidity.h"
#include <tuple>
#include <chrono>
#include <omp.h>

int calculateStress(const BoxConfiguration& body,
		             Kim& kim,
					 std::tuple<> stress,
                     const bool& projectForces=false)
{

	MY_WARNING("No stress calculation requested. Returning to caller.");
	return 1;

}

template<typename ...BF>
int calculateStress(const BoxConfiguration& body,
		             Kim& kim,
					 std::tuple<Stress<BF,Cauchy>&...> stress,
                     const bool& projectForces=false)
{
	std::tuple<> emptyTuple;
    return calculateStress(body,
                           kim,
                           emptyTuple,
                           stress,
                           projectForces);
    /*
	if (stressType == Piola)
		return calculateStress(body,
							   kim,
							   stress,
							   emptyTuple);
	else if (stressType == Cauchy)
		return calculateStress(body,
							   kim,
							   emptyTuple,
							   stress);
	else
		MY_ERROR("Unrecognized stress type " + std::to_string(stressType));
     */
}
template<typename ...BF>
int calculateStress(const BoxConfiguration& body,
                    Kim& kim,
                    std::tuple<Stress<BF,Piola>&...> stress,
                    const bool& projectForces=false)
{
    std::tuple<> emptyTuple;
    return calculateStress(body,
                           kim,
                           stress,
                           emptyTuple,
                           projectForces);
}

// This is the main driver of stress calculation
template<typename ...TStressPiola, typename ...TStressCauchy>
int calculateStress(const BoxConfiguration& body,
		             Kim& kim,
					 std::tuple<TStressPiola&...> piolaStress,
					 std::tuple<TStressCauchy&...> cauchyStress,
                     const bool& projectForces=false)
{
	int status= 0;
	int numberOfPiolaStresses= sizeof...(TStressPiola);
	int numberOfCauchyStresses= sizeof...(TStressCauchy);


	if (numberOfPiolaStresses == 0 && numberOfCauchyStresses == 0)
	{
		MY_WARNING("No stress calculation requested. Returning to caller.");
		return 1;
	}

	// At least one stress is being requested
	MY_BANNER("Begin stress calculation");
	auto start = std::chrono::system_clock::now();
	std::time_t startTime = std::chrono::system_clock::to_time_t(start);
	std::cout << "Time stamp: " << std::ctime(&startTime) << std::endl;

    // nullify the stress fields before starting
    recursiveNullifyStress(piolaStress);
    recursiveNullifyStress(cauchyStress);

	if (numberOfPiolaStresses > 0)
	{
		std::cout << "Number of Piola stresses requested : " << numberOfPiolaStresses << std::endl;
		std::cout << std::endl;
		std::cout << std::setw(25) << "Grid" << std::setw(25) << "Averaging domain size" << std::endl;
		auto referenceGridDomainSizePairs= getTGridDomainSizePairs(std::move(piolaStress));
		for (const auto& pair : referenceGridDomainSizePairs)
			std::cout <<  std::setw(25) << (GridBase*) pair.first << std::setw(25) << pair.second << std::endl;

		if (body.coordinates.at(Reference).rows() == 0)
		{
			MY_WARNING("No reference coordinates detected to compute Piola stress.");
			if (numberOfCauchyStresses>0)
			{
				MY_WARNING("Restarting stress calculation with only Cauchy stress.");
				status= calculateStress(body,kim,cauchyStress,projectForces);
				if (status==1)
					return status;
				else
					MY_ERROR("Error in stress computation.");
			}
			else
			{
				MY_BANNER("End of stress calculation");
				return 1;
			}
		}
	}

	if (numberOfCauchyStresses>0)
	{
		std::cout << "Number of Cauchy stresses requested: " << numberOfCauchyStresses << std::endl;
		std::cout << std::endl;
		std::cout << std::setw(25) << "Grid" << std::setw(25) << "Averaging domain size" << std::endl;
		auto currentGridDomainSizePairs= getTGridDomainSizePairs(cauchyStress);
		for (const auto& pair : currentGridDomainSizePairs)
			std::cout <<  std::setw(25) << (GridBase*) pair.first << std::setw(25) << pair.second << std::endl;


	}
	std::cout << std::endl;


	double maxAveragingDomainSize= std::max(averagingDomainSize_max(piolaStress),averagingDomainSize_max(cauchyStress));
	std::cout << "Maximum averaging domain size across all stresses = " << maxAveragingDomainSize << std::endl;

	if (body.pbc.any() == 1)
	{
		std::unique_ptr<const Configuration> pconfig;
		MY_HEADING("Generating padding atoms for periodic boundary conditions");

		double influenceDistance= kim.influenceDistance;
		pconfig.reset(body.getConfiguration(2*influenceDistance+maxAveragingDomainSize));
		std::cout << "Thickness of padding = 2 influence distance + maximum averaging domain size = "
				  << 2*influenceDistance+maxAveragingDomainSize << std::endl;
		std::cout << "Total number of atoms including padding atoms = " << pconfig->numberOfParticles << std::endl;
		std::cout << std::endl;


		// TODO Change step to accommodate non-orthogonal boundary conditions
		Vector3d origin= Vector3d::Zero();
		Vector3d step= body.box.diagonal();
		recursiveFold(origin,step,body.pbc,piolaStress);
		recursiveFold(origin,step,body.pbc,cauchyStress);

		status= calculateStress(pconfig.get(),kim,piolaStress,cauchyStress,projectForces);
	}
	else
	{
		status= calculateStress(&body,kim,piolaStress,cauchyStress,projectForces);
	}

//	recursiveWriteStressAndGrid(piolaStress);
//	recursiveWriteStressAndGrid(cauchyStress);

	if (status==1)
	{
		std::cout << std::endl;
		std::cout << std::endl;
		std::cout << std::endl;
		std::cout << std::endl;
		auto end= std::chrono::system_clock::now();
		std::time_t endTime= std::chrono::system_clock::to_time_t(end);
		std::chrono::duration<double> elapsedSeconds(end-start);
		std::cout << "Elapsed time: " << elapsedSeconds.count() << " seconds" << std::endl;
		std::cout << "End of simulation" << std::endl;
		MY_BANNER("End of stress calculation");
		return status;
	}
	else
		MY_ERROR("Error in stress computation.");

}

template<typename ...TStressPiola,typename ...TStressCauchy>
int calculateStress(const Configuration* pconfig,
		             Kim& kim,
					 std::tuple<TStressPiola&...> piolaStress,
					 std::tuple<TStressCauchy&...> cauchyStress,
                     const bool& projectForces=false)
{
	assert(!(kim.kim_ptr==nullptr) && "Model not initialized");
	double influenceDistance= kim.influenceDistance;
	int numberOfPiolaStresses= sizeof...(TStressPiola);
	int numberOfCauchyStresses= sizeof...(TStressCauchy);

//	------------------------------------------------------------------
//	Generate a local configuration
//	------------------------------------------------------------------
	MY_HEADING("Building a local configuration");

//  Mappings from set of unique grids to the set of maximum averaging domain sizes
	const auto referenceGridAveragingDomainSizeMap=
			recursiveGridMaxAveragingDomainSizeMap(piolaStress);
	const auto currentGridAveragingDomainSizeMap=
			recursiveGridMaxAveragingDomainSizeMap(cauchyStress);

	Stencil stencil(*pconfig);

	if (numberOfPiolaStresses>0)
	{
		std::cout << "Piola Stress" << std::endl;
		std::cout << std::setw(25) << "Unique grid" << std::setw(30) << "Max. Averaging domain size" << std::endl;
		std::cout << std::setw(25) << "-----------" << std::setw(30) << "--------------------------" << std::endl;
		for(const auto& [pgrid,domainSize] : referenceGridAveragingDomainSizeMap)
		{
			std::cout <<  std::setw(25) << (GridBase*)pgrid << std::setw(25) << domainSize << std::endl;
			stencil.expandStencil(pgrid,domainSize+influenceDistance,influenceDistance);
		}
	}

	if (numberOfCauchyStresses>0)
	{
		std::cout << "Cauchy Stress" << std::endl;
		std::cout << std::setw(25) << "Unique grid" << std::setw(30) << "Max. Averaging domain size" << std::endl;
		std::cout << std::setw(25) << "-----------" << std::setw(30) << "--------------------------" << std::endl;
		for(const auto& [pgrid,domainSize] : currentGridAveragingDomainSizeMap)
		{
			std::cout <<  std::setw(25) << (GridBase*)pgrid << std::setw(25) << domainSize << std::endl;
			stencil.expandStencil(pgrid,domainSize+influenceDistance,influenceDistance);
		}
	}

	std::cout << std::endl;
	std::cout << "Creating a local configuration using the above grids." << std::endl;
	SubConfiguration subconfig{stencil};
	int numberOfParticles= subconfig.numberOfParticles;
	if (numberOfParticles == 0)
	{
        std::string message= "All grids away from the current material. Stresses are identically zero. Returning to the caller.";
		//MY_ERROR(message);
		throw(std::runtime_error(message));
	}
	std::cout << "Number of particle in the local configuration = " << numberOfParticles << std::endl;
	std::cout << "Number of contributing particles = " << subconfig.particleContributing.sum() << std::endl;


//	------------------------------------------------------------------
//		Generate neighbor lists for all the grids
//	------------------------------------------------------------------
    std::vector<GridSubConfiguration<Reference>> neighborListsOfReferenceGridsOne;
    std::vector<GridSubConfiguration<Reference>> neighborListsOfReferenceGridsTwo;
    std::vector<GridSubConfiguration<Current>> neighborListsOfCurrentGridsOne;
    std::vector<GridSubConfiguration<Current>> neighborListsOfCurrentGridsTwo;

	auto referenceGridDomainSizePairs= getTGridDomainSizePairs(std::move(piolaStress));
	assert(numberOfPiolaStresses==referenceGridDomainSizePairs.size());

	auto currentGridDomainSizePairs= getTGridDomainSizePairs(cauchyStress);
	assert(numberOfCauchyStresses == currentGridDomainSizePairs.size());

	for(const auto& gridDomainSizePair : referenceGridDomainSizePairs)
	{
		const auto& pgrid= gridDomainSizePair.first;
		const auto& domainSize= gridDomainSizePair.second;
        neighborListsOfReferenceGridsOne.emplace_back( *pgrid,subconfig,domainSize+influenceDistance);
        neighborListsOfReferenceGridsTwo.emplace_back( *pgrid,subconfig,domainSize+2*influenceDistance);
	}
	for(const auto& gridDomainSizePair : currentGridDomainSizePairs)
	{
		const auto& pgrid= gridDomainSizePair.first;
		const auto& domainSize= gridDomainSizePair.second;
        neighborListsOfCurrentGridsOne.emplace_back(*pgrid,subconfig,domainSize+influenceDistance);
        neighborListsOfCurrentGridsTwo.emplace_back(*pgrid,subconfig,domainSize+2*influenceDistance);
	}
    assert(neighborListsOfCurrentGridsOne.size() == numberOfCauchyStresses &&
           neighborListsOfCurrentGridsTwo.size() == numberOfCauchyStresses &&
           neighborListsOfReferenceGridsOne.size() ==  numberOfPiolaStresses &&
           neighborListsOfReferenceGridsTwo.size() ==  numberOfPiolaStresses);

	std::vector<GridBase*> pgridListPiola= getBaseGridList(piolaStress);
	std::vector<GridBase*> pgridListCauchy= getBaseGridList(cauchyStress);
	std::vector<GridBase*>  pgridList;
	pgridList.reserve( pgridListPiola.size() + pgridListCauchy.size() );
	pgridList.insert( pgridList.end(), pgridListPiola.begin(), pgridListPiola.end() );
	pgridList.insert( pgridList.end(), pgridListCauchy.begin(), pgridListCauchy.end() );
	assert(pgridList.size() == numberOfPiolaStresses + numberOfCauchyStresses);



//	------------------------------------------------------------------
//	Building neighbor list for bonds
//	------------------------------------------------------------------
	double bondCutoff;
    bondCutoff= 2.0*influenceDistance;
    std::cout << "bond cutoff = " << bondCutoff << std::endl;

	NeighList* nlForBonds;
	nbl_initialize(&nlForBonds);
	nbl_build(nlForBonds,numberOfParticles,
				 subconfig.coordinates.at(Current).data(),
				 bondCutoff,
				 1,
				 &bondCutoff,
				 subconfig.particleContributing.data());
	InteratomicForces bonds(nlForBonds);

    if (!projectForces) {
        //	------------------------------------------------------------------
        //	Build neighbor list of particles
        //	------------------------------------------------------------------
        MY_HEADING("Building neighbor list");
        const double* cutoffs= kim.getCutoffs();
        int numberOfNeighborLists= kim.getNumberOfNeighborLists();

        // TODO: assert whenever the subconfiguration is empty
        NeighList* nl;
        nbl_initialize(&nl);
        nbl_build(nl,numberOfParticles,
                  subconfig.coordinates.at(Current).data(),
                  influenceDistance,
                  numberOfNeighborLists,
                  cutoffs,
                  subconfig.particleContributing.data());

        int neighborListSize= 0;
        for (int i_particle=0; i_particle<numberOfParticles; i_particle++)
            neighborListSize+= nl->lists->Nneighbors[i_particle];
        std::cout << "Size of neighbor list = " <<neighborListSize << std::endl;


    //	------------------------------------------------------------------
    //	Broadcast to model
    //	------------------------------------------------------------------
        MY_HEADING("Broadcasting to model");
        kim.broadcastToModel(&subconfig,
                             subconfig.particleContributing,
                             nullptr,
                             nl,
                             (KIM::Function *) &nbl_get_neigh,
                             &bonds,
                             (KIM::Function *) &process_DEDr);
        std::cout << "Done" << std::endl;

    //	------------------------------------------------------------------
    //	Compute forces
    //	------------------------------------------------------------------
        MY_HEADING("Computing forces");
        kim.compute();
        std::cout << "Done" << std::endl;
        nbl_clean(&nl);
    }
    else
    {
        bonds.dVidxj_dVjdxi.resize(bonds.fij.size(),Vector3d::Zero());
        MY_HEADING("Beginning local calculations")
        for(int i_particlei=0; i_particlei<subconfig.numberOfParticles; ++i_particlei)
        {
            // consider only contributing particles
            if(subconfig.particleContributing[i_particlei] == 0) continue;

            // stencil out particle and its neighborhood
            Stencil singleParticleStencil(subconfig);
            std::vector<Vector3d> centerParticleCoordinates;
            centerParticleCoordinates.push_back(subconfig.coordinates.at(Current).row(i_particlei));
            //singleParticleStencil.expandStencil(centerParticleCoordinates,subconfig.coordinates.at(Current),0.0,influenceDistance);
            singleParticleStencil.expandStencil(centerParticleCoordinates,subconfig.coordinates.at(Current),0,influenceDistance);
            SubConfiguration subconfigOfParticle{singleParticleStencil};

            // form neighborlist of the particle
            const double* cutoffs= kim.getCutoffs();
            int numberOfNeighborLists= kim.getNumberOfNeighborLists();

            // TODO: assert whenever the subconfiguration is empty
            NeighList* nlOfParticle;
            nbl_initialize(&nlOfParticle);
            nbl_build(nlOfParticle,subconfigOfParticle.numberOfParticles,
                      subconfigOfParticle.coordinates.at(Current).data(),
                      influenceDistance,
                      numberOfNeighborLists,
                      cutoffs,
                      subconfigOfParticle.particleContributing.data());

            MatrixXd forces(subconfigOfParticle.numberOfParticles,DIM);
            forces.setZero();

            // broadcast to model
            kim.broadcastToModel(&subconfigOfParticle,
                                 subconfigOfParticle.particleContributing,
                                 &forces,
                                 nlOfParticle,
                                 (KIM::Function *) &nbl_get_neigh,
                                 nullptr,
                                 nullptr);
            // compute partial forces
            kim.compute();

            InteratomicForces localBonds(nlOfParticle);
            // loop over the neighbors of  center particle and collect all interatomic forces
            // We do not know if the center particle is numbered 0
            for (int i_local=0; i_local<subconfigOfParticle.numberOfParticles; ++i_local) {
                assert(i_particlei == subconfigOfParticle.localGlobalMap.at(i_local));
                for (int i_neighbor = 0; i_neighbor < localBonds.nlOne_ptr->Nneighbors[i_local]; ++i_neighbor) {
                    int index = localBonds.nlOne_ptr->beginIndex[i_local] + i_neighbor;
                    int jLocal= localBonds.nlOne_ptr->neighborList[index];
                    int i_particlej= subconfigOfParticle.localGlobalMap.at(jLocal);

                    // look for particlej in the neighborhood of particlei in bonds
                    for(int i_neighborOfi=0; i_neighborOfi<bonds.nlOne_ptr->Nneighbors[i_particlei]; ++i_neighborOfi) {
                        index = bonds.nlOne_ptr->beginIndex[i_particlei] + i_neighborOfi;
                        if(i_particlej ==  bonds.nlOne_ptr->neighborList[index]) {
                            // add force contribution dVi/dxj to fij
                            bonds.dVidxj_dVjdxi[index] += forces.row(jLocal);
                            break;
                        }
                    }

                    // look for particlei in the neighborhood of particlej
                    for(int i_neighborOfj=0; i_neighborOfj<bonds.nlOne_ptr->Nneighbors[i_particlej]; ++i_neighborOfj)
                    {
                        index= bonds.nlOne_ptr->beginIndex[i_particlej]+i_neighborOfj;
                        if (i_particlei == bonds.nlOne_ptr->neighborList[index]){
                            // add force contribution dVi/dxj to fji:= dVj/dxi - dVi/dxj
                            bonds.dVidxj_dVjdxi[index] += -forces.row(jLocal);
                            break;
                        }
                    }
                }
            }
            nbl_clean(&nlOfParticle);
        }
        std::cout << "Done" << std::endl;


        // At this point, dVi/dxj-dVj/dxi is antisymmetric only for pairs of contributing atoms.
        MatrixXd gi(numberOfParticles,DIM);
        gi.setZero();
        for(int i_particlei=0; i_particlei<subconfig.numberOfParticles; ++i_particlei)
        {
            if(subconfig.particleContributing[i_particlei] == 0) continue;
            for(int i_neighborOfi=0; i_neighborOfi<bonds.nlOne_ptr->Nneighbors[i_particlei]; ++i_neighborOfi)
            {
                int index= bonds.nlOne_ptr->beginIndex[i_particlei]+i_neighborOfi;
                int i_particlej= bonds.nlOne_ptr->neighborList[index];
                Vector3d particlei= subconfig.coordinates.at(Current).row(i_particlei);
                Vector3d particlej= subconfig.coordinates.at(Current).row(i_particlej);
                Vector3d eij= (particlei-particlej).normalized();
                bonds.fij[index]= bonds.dVidxj_dVjdxi[index].dot(eij);

                Vector3d fijPerp= bonds.dVidxj_dVjdxi[index]- bonds.fij[index]*eij;
                // sum over all the perpendicular components
                gi.row(i_particlei)+= fijPerp;

                // account for perpendicular forces on non-contributing particles
                if(subconfig.particleContributing[i_particlej] == 0)
                    gi.row(i_particlej)-= fijPerp;
            }
        }
        std::vector<double> hij(bonds.fij.size(),0.0);
        if (gi.reshaped<Eigen::RowMajor>().norm() > 1e-6) {
            Rigidity rigidity(subconfig.coordinates.at(Current), bonds.nlOne_ptr);
            hij = rigidity.project(gi.reshaped<Eigen::RowMajor>());
        }

        // update: fij
        int index= 0;
        for(auto& element : bonds.fij) {
            element += hij[index];
            index++;
        }

        MatrixXd fi(numberOfParticles,DIM);
        fi.setZero();
        for(int i_particlei=0; i_particlei<subconfig.numberOfParticles; ++i_particlei) {
            if (subconfig.particleContributing[i_particlei] == 0) continue;
            for (int i_neighborOfi = 0; i_neighborOfi < bonds.nlOne_ptr->Nneighbors[i_particlei]; ++i_neighborOfi) {
                int index = bonds.nlOne_ptr->beginIndex[i_particlei] + i_neighborOfi;
                int i_particlej = bonds.nlOne_ptr->neighborList[index];
                Vector3d particlei = subconfig.coordinates.at(Current).row(i_particlei);
                Vector3d particlej = subconfig.coordinates.at(Current).row(i_particlej);
                Vector3d eij = (particlei - particlej).normalized();
                fi.row(i_particlei) += hij[index] * eij;
            }
        }

        double maxError=0;
        for(int i_particlei=0; i_particlei<subconfig.numberOfParticles; ++i_particlei) {
            if (subconfig.particleContributing[i_particlei] == 0) continue;
            maxError= std::max(maxError,(gi.row(i_particlei)-fi.row(i_particlei)).norm());
        }
        std::cout << "Maximum error = " << maxError << std::endl;
    }

//	------------------------------------------------------------------
//	Loop over local grid points and accumulate stress
//	------------------------------------------------------------------
	MY_HEADING("Looping over grids");

	int i_grid= 0;
	for(const auto& pgrid : pgridList)
	{
		//int i_gridPoint= 0;
		double progress= 0;
		int numberOfGridPoints= pgrid->coordinates.size();
		std::cout << i_grid+1 << ". Number of grid points: " << numberOfGridPoints << std::endl;

        #pragma omp parallel for
		//for (const auto& gridPoint : pgrid->coordinates)
        for(int i_gridPoint=0; i_gridPoint<numberOfGridPoints; i_gridPoint++)
		{
            const auto& gridPoint= pgrid->coordinates[i_gridPoint];
			if ( numberOfGridPoints<10 || (i_gridPoint+1)%(numberOfGridPoints/10) == 0)
			{
				progress= (double)(i_gridPoint+1)/numberOfGridPoints;
				progressBar(progress);
			}
            std::set<int> neighborListOne, neighborListTwo;
            if (i_grid<numberOfPiolaStresses)
            {
                neighborListOne= neighborListsOfReferenceGridsOne[i_grid].getGridPointNeighbors(i_gridPoint);
                neighborListTwo= neighborListsOfReferenceGridsTwo[i_grid].getGridPointNeighbors(i_gridPoint);
            }
            else
            {
                neighborListOne= neighborListsOfCurrentGridsOne[i_grid-numberOfPiolaStresses].getGridPointNeighbors(i_gridPoint);
                neighborListTwo= neighborListsOfCurrentGridsTwo[i_grid-numberOfPiolaStresses].getGridPointNeighbors(i_gridPoint);
            }
			for (const auto& particle1 : neighborListOne)
			{
                Vector3d ra,rA,rb,rB,rab,rAB;
                ra= rA= rb= rB= rab= rAB= Vector3d::Zero();

				if(numberOfPiolaStresses>0) rA= subconfig.coordinates.at(Reference).row(particle1) - gridPoint;
				ra= subconfig.coordinates.at(Current).row(particle1) - gridPoint;
				int index;
				int numberOfNeighborsOf1= bonds.nlOne_ptr->Nneighbors[particle1];

//				Loop through the bond neighbors of particle1
				for(int i_neighborOf1= 0; i_neighborOf1<numberOfNeighborsOf1; i_neighborOf1++)
				{
					index= bonds.nlOne_ptr->beginIndex[particle1]+i_neighborOf1;
					double fij= bonds.fij[index];
					if (fij == 0) continue;

//					At this point, the force in the bond connecting particles 1 and 2 is nonzero
					int particle2= bonds.nlOne_ptr->neighborList[index];
					if(numberOfPiolaStresses>0) rB= subconfig.coordinates.at(Reference).row(particle2) - gridPoint;
					rb= subconfig.coordinates.at(Current).row(particle2) - gridPoint;
//					Ignore if (particle2 is in neighborListOne and particle 1 > particle 2) as this pair
//					is encountered twice
					if ( (neighborListOne.find(particle2) != neighborListOne.end() && particle1 < particle2))
						continue;

//					Since neighborListOne \subset of neighborListTwo, the following condition if(A||B)
//					is equivalent if(B). Nevertheless, we have if(A||B) since B is expensive, and it is never
//					evaluated if A is true.

					if ( neighborListOne.find(particle2) != neighborListOne.end()  ||
						 neighborListTwo.find(particle2) != neighborListTwo.end())
					{
						if(numberOfPiolaStresses>0) rAB= subconfig.coordinates.at(Reference).row(particle1)-subconfig.coordinates.at(Reference).row(particle2);
						rab= subconfig.coordinates.at(Current).row(particle1)-subconfig.coordinates.at(Current).row(particle2);
						if (i_grid<numberOfPiolaStresses)
							recursiveBuildStress(fij,ra,rA,rb,rB,rab,rAB,i_gridPoint,i_grid,piolaStress);
						else
							recursiveBuildStress(fij,ra,rA,rb,rB,rab,rAB,i_gridPoint,i_grid-numberOfPiolaStresses,cauchyStress);
					}
				}
			}
			//i_gridpoint++;
		}
		std::cout << "Done with grid " << pgrid << std::endl;
		std::cout << std::endl;

		i_grid++;
	}

	// Collect all the stress fields in processor 0

	nbl_clean(&nlForBonds);
	return 1;
}


int process_DEDr(const void* dataObject, const double de, const double r, const double* const dx, const int i, const int j)
 {
	InteratomicForces* bonds_ptr= (InteratomicForces*) dataObject;
	int index;
	int numberOfNeighborsOfi = bonds_ptr->nlOne_ptr->Nneighbors[i];
	int numberOfNeighborsOfj = bonds_ptr->nlOne_ptr->Nneighbors[j];
	bool iFound= false;
	bool jFound= false;

	// Look for j in the neighbor list of i
	for(int i_neighborOfi= 0; i_neighborOfi<numberOfNeighborsOfi; i_neighborOfi++)
	{
		index= bonds_ptr->nlOne_ptr->beginIndex[i]+i_neighborOfi;
		if(bonds_ptr->nlOne_ptr->neighborList[index] == j)
		{
			bonds_ptr->fij[index]+= de;
			jFound= true;
			break;
		}
	}
	// Look for i in the neighbor list of j
	for(int i_neighborOfj= 0; i_neighborOfj<numberOfNeighborsOfj; i_neighborOfj++)
	{
		index= bonds_ptr->nlOne_ptr->beginIndex[j]+i_neighborOfj;
		if(bonds_ptr->nlOne_ptr->neighborList[index] == i)
		{
			bonds_ptr->fij[index]+= de;
			iFound= true;
			break;
		}

	}
	return 0;
 }
