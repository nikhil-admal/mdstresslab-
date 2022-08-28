/*
 * main.cpp
 *
 *  Created on: Nov 3, 2019
 *      Author: Nikhil Admal
 */
#include "MethodLdad.h"
#include <string>
#include <iostream>
#include <tuple>
#include <fstream>
#include "BoxConfiguration.h"
#include "calculateStress.h"
#include "Grid.h"
#include "typedef.h"
#include "../compareStress.cpp"


int main()
{
	int numberOfParticles;
	int referenceAndFinal= true;
	std::string configFileName= "config.data";
	std::string modelname= "SW_StillingerWeber_1985_Si__MO_405512056662_005";

//	-------------------------------------------------------------------
// Input configuration and potential
//	-------------------------------------------------------------------

	std::ifstream file(configFileName);
	if(!file) MY_ERROR("ERROR: config.dat could not be opened for reading!");

	file >> numberOfParticles;
	if (numberOfParticles < 0) MY_ERROR("Error: Negative number of particles.\n");

	BoxConfiguration body{numberOfParticles,referenceAndFinal};
	body.read(configFileName,referenceAndFinal);

	Kim kim(modelname);

//	-------------------------------------------------------------------
// Create grid
//	-------------------------------------------------------------------

	int ngrid = 125;
	Grid<Reference> gridFromFile_ref(ngrid);
	gridFromFile_ref.read("grid_pk1.data");

    Grid<Current> gridFromFile_def(ngrid);
	gridFromFile_def.read("grid_cauchy.data");

//	-------------------------------------------------------------------
// Calculate stress on the grid
//	-------------------------------------------------------------------
	// Create hardyStress object

    Matrix3d ldadVectors_ref;

    ldadVectors_ref << 5.43094977840521, 0.0, 0.0, 
                       0.0, 5.43094977840521, 0.0,
                       0.0,  0.0, 5.43094977840521;   

	// Ldad stress ref
	MethodLdadConstant ldad_constant_ref(ldadVectors_ref);
    MethodLdadTrigonometric ldad_trigonometric_ref(ldadVectors_ref);

	//TODO The bond function should be accepted as a reference
	Stress<MethodLdadConstant,Piola> ldad_constant_stress_ref("ldad_constant_ref",ldad_constant_ref,&gridFromFile_ref);
    Stress<MethodLdadTrigonometric,Piola> ldad_trigonometric_stress_ref("ldad_trigonometric_ref",ldad_trigonometric_ref,&gridFromFile_ref);

	calculateStress(body,kim,std::tie(ldad_constant_stress_ref));
    ldad_constant_stress_ref.write();
	calculateStress(body,kim,std::tie(ldad_trigonometric_stress_ref));
    ldad_trigonometric_stress_ref.write();

    Matrix3d ldadVectors_def;

    ldadVectors_def << 5.43094977840521, 0.0, 0.0,
	                   0.0, 5.4852592761892621, 0.0,
					   0.0, 0.0, 5.43094977840521;

	// Ldad stress def
	MethodLdadConstant ldad_constant_def(ldadVectors_def);
    MethodLdadTrigonometric ldad_trigonometric_def(ldadVectors_def);

	Stress<MethodLdadConstant,Cauchy> ldad_constant_stress_def("ldad_constant_def",ldad_constant_def,&gridFromFile_def);
    Stress<MethodLdadTrigonometric,Cauchy> ldad_trigonometric_stress_def("ldad_trigonometric_def",ldad_trigonometric_def,&gridFromFile_def);

	calculateStress(body,kim,std::tie(ldad_constant_stress_def));
    ldad_constant_stress_def.write();
	calculateStress(body,kim,std::tie(ldad_trigonometric_stress_def));
    ldad_trigonometric_stress_def.write();

	compareStress("ldad_constant_ref");
	compareStress("ldad_constant_def");
	compareStress("ldad_trigonometric_ref");
	compareStress("ldad_trigonometric_def");
	return 0;
}


