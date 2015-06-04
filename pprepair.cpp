/*
 Copyright (c) 2009-2014,
 Ken Arroyo Ohori    g.a.k.arroyoohori@tudelft.nl
 Hugo Ledoux         h.ledoux@tudelft.nl
 Martijn Meijers     b.m.meijers@tudelft.nl
 All rights reserved.
 
 This file is part of pprepair: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 Licensees holding a valid commercial license may use this file in
 accordance with the commercial license agreement provided with
 the software.
 
 This file is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "PlanarPartition.h"
#include <fstream>
#include <iostream>
#include <tclap/CmdLine.h>

class MyOutput : public TCLAP::StdOutput
{
public:
  
  virtual void usage(TCLAP::CmdLineInterface& c)
  {
    std::cout << "===== pprepair =====" << std::endl;
    std::cout << "OPTIONS" << std::endl;
    std::list<TCLAP::Arg*> args = c.getArgList();
    for (TCLAP::ArgListIterator it = args.begin(); it != args.end(); it++) {
      if ((*it)->getFlag() == "")
        std::cout << "\t--" << (*it)->getName() << std::endl;
      else
        std::cout << "\t-" << (*it)->getFlag() << ", --" << (*it)->getName() << std::endl;
      std::cout << "\t\t" << (*it)->getDescription() << std::endl;
    }
    std::cout << "EXAMPLES" << std::endl;
    std::cout << "\tpprepair -i file1.shp -i file2.geojson --outerrors out.shp -v" << std::endl;
    std::cout << "\t\tTakes 2 input files, validates them," << std::endl;
    std::cout << "\t\tand output the problematic regions to out.shp" << std::endl << std::endl;
    std::cout << "\tpprepair -i file1.shp -i file2.geojson -o /home/elvis/temp/ -r fix" << std::endl;
    std::cout << "\t\tTakes 2 input files, repairs them with the default method (RandomNeighbour)" << std::endl;
    std::cout << "\t\tand outputs 2 repaired shapefiles to /home/elvis/temp/ folder" << std::endl;
    std::cout << "\t\t(file1.r.shp and file2.r.shp)" << std::endl << std::endl;
    std::cout << "\tpprepair -i file1.shp -o /home/elvis/temp/ -r PL --priority prio.txt" << std::endl;
    std::cout << "\t\tTakes 1 input file, repairs it with PriorityList rule" << std::endl;
    std::cout << "\t\tand outputs the repaired shapefile to /home/elvis/temp/ folder" << std::endl << std::endl;
    std::cout << "\tpprepair -i file1.shp -e extent.geojson -o . -r LB" << std::endl;
    std::cout << "\t\tTakes 1 input file and a spatial extent file," << std::endl; 
    std::cout << "\t\trepairs file1.shp for holes and overlaps + 'aligns' it to extent.geojson" << std::endl; 
    std::cout << "\t\tRepaired shapefile file1.r.shp saved to current folder" << std::endl << std::endl;
  }
};


int main (int argc, char* const argv[]) {
  
  std::vector<std::string> repairMethods;
  repairMethods.push_back("fix");  //-- fix == random neighbour
  repairMethods.push_back("RN");   //-- random neighbour
  repairMethods.push_back("LB");   //-- longest boundary
  repairMethods.push_back("PL");   //-- priority list
  repairMethods.push_back("EM"); //-- edge-matching with priority given either by (1) attributes or (2) datasets
  TCLAP::ValuesConstraint<std::string> rmVals(repairMethods);

  TCLAP::CmdLine cmd("Allowed options", ' ', "");
  MyOutput my;
  cmd.setOutput(&my);
  try {
    TCLAP::MultiArg<std::string> inputDSs          ("i", "input", "input OGR dataset (this can be used multiple times)", true, "string");
    TCLAP::ValueArg<std::string> outfiles          ("o", "output", "folder for repaired shapefile(s))", false, "","string");
    TCLAP::ValueArg<std::string> extent            ("e", "extent", "spatial extent (OGR dataset containing *one* polygon)", false, "", "string");
    TCLAP::SwitchArg             validation        ("v", "validation", "validation only (gaps and overlaps reported)", false);
    TCLAP::SwitchArg             skipvalideach     ("",  "skipvalideach", "Skip the individual validation of each input polygon (activated by default)", false);
    TCLAP::ValueArg<std::string> repair            ("r", "repair", "repair method used: <fix|RN|LB|PL|EM>", false, "", &rmVals);
    TCLAP::ValueArg<std::string> priority          ("p", "prio", "priority list for repairing (methods <PL|EM>)", false, "", "string");
    TCLAP::ValueArg<double>      splitregions      ("",  "splitregions", "maximum distance for inserting new constraints when splitting", false, -1.0, "double");
    
    TCLAP::ValueArg<std::string> outerrors         ("",  "outerrors", "output errors (SHP file)", false, "","string");
    TCLAP::ValueArg<std::string> outerrorslist     ("",  "outerrorslist", "output list of errors (CSV file)", false, "", "string");
    TCLAP::ValueArg<std::string> outtr             ("",  "outtr", "output triangulation (SHP file)", false, "","string");
    
    TCLAP::ValueArg<float>       elfslivers        ("",  "elf", "ignore holes that are not slivers (provide minarea)", false, -1.0, "float");
    TCLAP::ValueArg<int>         rounding          ("",  "rounding", "rounding input coordinates to 1eX (you pass X)", false, -9999, "int");

    
    cmd.add(elfslivers);
    cmd.add(rounding);
    cmd.add(outerrorslist);
    cmd.add(outerrors);
    cmd.add(outtr);
    cmd.add(extent);
    cmd.add(priority);
    cmd.add(skipvalideach);
    cmd.add(splitregions);
    cmd.xorAdd(validation, repair);
    cmd.add(outfiles);
    cmd.add(inputDSs);
    cmd.parse( argc, argv );
    
    //-- add input datasets to PP
    PlanarPartition pp(rounding.getValue());
    std::vector<std::string> inputs = inputDSs.getValue();
    for (std::vector<std::string>::iterator it = inputs.begin() ; it != inputs.end(); ++it) {
      if (pp.addOGRdataset(*it, skipvalideach.getValue()) == false) {
        std::string s("Some polygons are (individually) invalid.");
        throw s;
      }
      
    }
    std::cout << std::endl << "Total input polygons: " << pp.noPolygons() << std::endl;
    //-- add spatial extent
    if (extent.getValue() != "") {
      if (pp.addOGRdatasetExtent(extent.getValue()) == false)
        throw false;
    }

    //-- tag the triangulation
    pp.buildPP();
    
    //-- validation only
    if (validation.getValue() == true) {
      if (pp.isValid() == false) {
        std::cout << "\nValidation:\n\t planar partition is NOT valid.\n" << std::endl;
        pp.printTriangulationInfo();
        pp.printProblemRegions();
        if (outerrorslist.getValue() != "") {
          std::ofstream outfile;
          outfile.open((outerrorslist.getValue()).c_str());
          pp.getListOverlappingPolygons(outfile);
          outfile.close();
          std::cout << std::endl << "CSV file with overlapping polygons written to " << outerrorslist.getValue() << std::endl;
        }
        if (outerrors.getValue() != "") {
          if (elfslivers.getValue() == -1.0) {
            pp.exportProblemRegionsAsSHP(outerrors.getValue());
          }
          else {
            pp.exportProblemRegionsAsSHP(outerrors.getValue(), 0.3, elfslivers.getValue());
          }
        }
      }
      else {
        std::cout << "\nValidation:\n\t planar partition is valid." << std::endl;
        pp.printTriangulationInfo();
      }
    }
    else { //-- repairing
      pp.printTriangulationInfo();
      pp.printProblemRegions();
      if (outerrors.getValue() != "") {
        pp.exportProblemRegionsAsSHP(outerrors.getValue());
      }
      
      if ( (repair.getValue() == "PL") || (repair.getValue() == "EM") ){
        if (priority.getValue() == "") {
          std::cout << "Priority file must be provided." << std::endl;
          throw false;
        }
        else {
          if (pp.repair(repair.getValue(), true, priority.getValue(), splitregions.getValue()) == false) {
            throw false;
          }
        }
      }
      else {
        pp.repair(repair.getValue());
      }
      //-- if there was a 'tie' then fix with RN
      if (pp.isValid() == false) {
        std::cout << "Reparing 'ties'..." << std::endl;
        pp.repair("RN");
      }
      pp.printTriangulationInfo();
      pp.printProblemRegions();
      
      //-- output repaired SHP files
      if (outfiles.getValue() != "") {
        pp.reconstructPolygons();
        if (pp.exportPolygonsSHP(outfiles.getValue()) == false) {
          return(0);
        }
      }
      else {
        std::cout << "Results not saved anywhere (option '-o' wasn't set)" << std::endl;
      }
    }
    //-- output triangulation in SHP
    if (outtr.getValue() != "") {
      pp.exportTriangulation(outtr.getValue());
    }
	}
  catch (TCLAP::ArgException &e) {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    return(0);
  }
  catch (std::string problem) {
    std::cerr << std::endl << "ERROR: " << problem << " (our other project 'prepair' can perform automatic repair of single polygons)" << std::endl;
    std::cerr << "Aborted." << std::endl;
    return(0);
  }
  catch (bool b) {
    std::cerr << "Aborted." << std::endl;
    return(0);
  }
  std::cout << "\nSuccessfully terminated." << std::endl;
  return(1);
}



