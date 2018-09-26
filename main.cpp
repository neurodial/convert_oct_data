/*
 * Copyright (c) 2018 Kay Gawlik <kay.gawlik@charite.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <iostream>

#include<boost/filesystem.hpp>
#include<boost/lexical_cast.hpp>
#include<boost/program_options.hpp>

#include<octdata/octfileread.h>
#include<octdata/filereadoptions.h>
#include<octdata/datastruct/oct.h>
#include<octdata/filewriteoptions.h>

namespace bfs = boost::filesystem;
namespace bpo = boost::program_options;


enum class OutputFormat {xoct, octbin, img};
struct Options
{
	OutputFormat outputFormat = OutputFormat::xoct;
	bool addOldFilename = false;
	bool anonymising    = false;
	bfs::path outputPath;

	OctData::FileReadOptions  optFileRead;
	OctData::FileWriteOptions optWrite;
};

const char* getExtention(OutputFormat format)
{
	switch(format)
	{
		case OutputFormat::xoct  : return ".xoct"  ;
		case OutputFormat::octbin: return ".octbin";
		case OutputFormat::img   : return ".img"   ;
	}
	return "";
}


std::string createFilename(const OctData::OCT& octdata, const bfs::path& sourceFilename, const Options& opt)
{
	const std::string oldFileName = sourceFilename.stem().generic_string();

	if(octdata.size() == 0)
		return oldFileName;

	OctData::OCT::SubstructureCIterator pat = octdata.begin();
	const OctData::Patient* p = pat->second;

	if(!p || p->size() == 0)
		return oldFileName;
	OctData::Patient::SubstructureCIterator study = p->begin();
	const OctData::Study* s = study->second;
	const int studyId = study->first;

	std::string destFileName = p->getId();
	if(destFileName.empty())
		destFileName = "unknown";


	if(!s || s->size() == 0)
		return destFileName;
	OctData::Study::SubstructureCIterator series = s->begin();
	const int seriesId = series->first;


	destFileName += "_" + boost::lexical_cast<std::string>(studyId) + "_" + boost::lexical_cast<std::string>(seriesId);
	if(opt.addOldFilename)
		destFileName += '_' + sourceFilename.stem().generic_string();

	return destFileName;
}

void anonymisingOct(OctData::OCT& octData)
{
	for(OctData::OCT::SubstructurePair& pair : octData)
	{
		OctData::Patient* pat = pair.second;
		if(pat)
		{
			pat->setSurname (std::string());
			pat->setForename(std::string());
			pat->setTitle   (std::string());

			OctData::Date d = pat->getBirthdate();
			if(!d.isEmpty())
			{
				d.setDay(1);
				d.setMonth(1);

				pat->setBirthdate(d);
			}
		}
	}
}

void convertFile(const bfs::path& filename, const Options& opt)
{
	OctData::OCT octdata = OctData::OctFileRead::openFile(filename, opt.optFileRead);

	bfs::path destFileName = createFilename(octdata, filename, opt);

	if(opt.outputPath.empty())
		destFileName = filename.branch_path() / destFileName;
	else
		destFileName = opt.outputPath / destFileName;

	destFileName += getExtention(opt.outputFormat);
	if(bfs::exists(destFileName))
	{
		std::cerr << "Fehler: Zieldatei existiert: " << destFileName << std::endl;
		return;
	}

	std::cout << "Zieldatei: " << destFileName << std::endl;
	if(!destFileName.empty())
	{
		if(opt.anonymising)
			anonymisingOct(octdata);

		bool writeSuccess = OctData::OctFileRead::writeFile(destFileName, octdata, opt.optWrite);
		if(!writeSuccess)
			std::cerr << "Error: write file " << destFileName << " not successful\n";
	}
}


void convertFilesFromDir(const bfs::path& search_here, const Options& opt)
{
	bfs::recursive_directory_iterator dir(search_here);
	bfs::recursive_directory_iterator end;
	while(dir != end)
	{
		if(!bfs::is_directory(*dir))
		{
			if(dir->path().extension() != getExtention(opt.outputFormat))
			{
				std::string filename = dir->path().generic_string();
				if(OctData::OctFileRead::isLoadable(filename))
					convertFile(filename, opt);
			}
		}
		++dir;
	}
 }

int main(int argc, char** argv)
{
	bfs::path mainPath = bfs::path(argv[0]).branch_path();

	std::string              outputFormatString;
	std::vector<std::string> octPaths;

	Options opt;

	/**
	 * Define and parse the program options
	*/
	bpo::options_description options("Options");
	options.add_options()
	        ("help,h"                                                                , "Print help messages"              )
	        ("octpath"       , bpo::value(&octPaths)->required()                     , "one or more oct scan"             )
	        ("addOldFilename"                                                        , "add old filename at the end"      )
	        ("outputPath"    , bpo::value(&opt.outputPath)                           , "put files in this folder"         )
	        ("anonymising,a"                                                         , "strip patient name"               )
	        ("outputformat,f", bpo::value(&outputFormatString)->default_value("xoct"), "Output format (xoct, octbin, img)");


	bpo::positional_options_description file_options;
	file_options.add("octpath", -1);

	bpo::variables_map vm;
	try
	{
		bpo::store(bpo::command_line_parser(argc, argv).options(options).positional(file_options).run(), vm); // can throw

		if(vm.count("help"))
		{
			std::cout << "Convert all files in octpath to the outputformat\n" << options << std::endl;
			return 0;
		}

		bpo::notify(vm); // throws on error, so do after help in case
		                 // there are any problems

		opt.anonymising    = (vm.count("anonymising"   ) > 0);
		opt.addOldFilename = (vm.count("addOldFilename") > 0);

	}
	catch(bpo::error& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
		std::cerr << options << std::endl;
		return 1;
	}

	     if(outputFormatString == "xoct"  ) opt.outputFormat = OutputFormat::xoct  ;
	else if(outputFormatString == "octbin") opt.outputFormat = OutputFormat::octbin;
	else if(outputFormatString == "img"   ) opt.outputFormat = OutputFormat::img   ;
	else
	{
		std::cerr << "ERROR: Wrong output format: " << outputFormatString << '\n' << std::endl;
		std::cerr << options << std::endl;
		return 1;
	}


	opt.optFileRead.fillEmptyPixelWhite = false;
	opt.optFileRead.registerBScanns     = true;
	opt.optFileRead.libPath             = mainPath.generic_string();

	opt.optWrite.octBinFlat = true;


	for(const std::string& octPath : octPaths)
	{
		bfs::path filename(octPath);
		if(bfs::is_directory(filename))
			convertFilesFromDir(filename, opt);
		else
			convertFile(filename, opt);
	}
}
