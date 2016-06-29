#include "mzSample.h"

//global options
int mzSample::filter_minIntensity = -1;
bool mzSample::filter_centroidScans = false;
int mzSample::filter_intensityQuantile = 0;
int mzSample::filter_polarity=0;
int mzSample::filter_mslevel=0;


mzSample::mzSample() {
    maxMz = maxRt = 0;
    minMz = minRt = 0;
    isBlank = false;
    isSelected=true;
    maxIntensity=0;
    minIntensity=0;
    totalIntensity=0;
    _normalizationConstant=1;
    _sampleOrder=0;
    _C13Labeled=false;
    _N15Labeled=false;
    _S34Labeled=false; //Feng note: added to track S34 labeling state
    _D2Labeled=false; //Feng note: added to track D2 labeling state
    _setName =  "A";
    color[0]=color[1]=color[2]=0;
    color[3]=1.0;
}

mzSample::~mzSample() { 
        for(unsigned int i=0; i < scans.size(); i++ )
            if(scans[i]!=NULL) delete(scans[i]);
        scans.clear();
}

void mzSample::addScan(Scan*s ) {
        if (!s) return;

	//skip scans that do not match mslevel
	if (mzSample::filter_mslevel and s->mslevel != mzSample::filter_mslevel ) {
		return;
	}
	//skip scans that do not match polarity 
	if (mzSample::filter_polarity and s->getPolarity() != mzSample::filter_polarity ) {
		return;
	}

        //unsigned int sizeBefore = s->intensity.size();
        if ( mzSample::filter_centroidScans == true ) {
            s->simpleCentroid();
        }

        //unsigned int sizeAfter1 = s->intensity.size();

        if ( mzSample::filter_intensityQuantile > 0) {
            s->quantileFilter(mzSample::filter_intensityQuantile);
        }
        //unsigned int sizeAfter2 = s->intensity.size();

        if ( mzSample::filter_minIntensity > 0) {
            s->intensityFilter(mzSample::filter_minIntensity);
        }
        //unsigned int sizeAfter3 = s->intensity.size();
        //cerr << "addScan " << sizeBefore <<  " " << sizeAfter1 << " " << sizeAfter2 << " " << sizeAfter3 << endl;

        scans.push_back(s);
        s->scannum=scans.size()-1;
}

void mzSample::loadSample(const char* filename) {
		if (mystrcasestr(filename,"mzCSV") != NULL ) {
                        parseMzCSV(filename);
		} else if(mystrcasestr(filename,"mzdata") != NULL ) {
                        parseMzData(filename);
                } else if(mystrcasestr(filename,"mzxml") != NULL ) {
                    parseMzXML(filename);
                } else if(mystrcasestr(filename,"mzml") != NULL ) {
                    parseMzML(filename);
                } else if(mystrcasestr(filename,"cdf") != NULL ) {
                    parseCDF(filename,1);
                } else {
                    parseMzData(filename);
                }

		enumerateSRMScans();

		//set min and max values for rt
		calculateMzRtRange();

		//check if this is a blank sample
		string filenameString = string(filename);
		this->fileName = filenameString;
		makeLowerCase(filenameString);
		if ( filenameString.find("blan") != string::npos) { 
			this->isBlank = true;
			cerr << "Found Blank: " << filenameString << endl; 
		}
}

void mzSample::parseMzCSV(const char* filename) {
		// file structure: scannum,rt,mz,intensity,mslevel,precursorMz,polarity,srmid
		int lineNum=0; 
		cerr << "Loading " << filename << endl;
		ifstream myfile(filename);
		if (! myfile.is_open() ) { cerr << "Can't open file " << filename; return; }

	    std::stringstream ss;
            std::string line;
            std::string polarity;

			int lastScanNum=-1;
			int scannum=0;
			float rt=0;
			float intensity=0;
			float mz=0;
			float precursorMz=0;
			int mslevel=0;

	   Scan* scan = NULL;
            int newscannum=0;

            while ( getline(myfile,line) ) {
                lineNum++;
                vector<string>fields;
                mzUtils::split(line,',', fields); 
                if (fields.size() >= 5 && lineNum > 1) {

                    ss.clear();

                    ss  << fields[0] <<  " " 
                        << fields[1] <<  " "
                        << fields[2] <<  " " 
                        << fields[3] <<  " "
                        << fields[4] << " "
                        << fields[5] << " "
                        << fields[6];

                    ss >> scannum >> rt >> mz >> intensity >> mslevel >> precursorMz >> polarity;

                    if ( scannum != lastScanNum ) {
                        newscannum++;
                        if (mslevel <= 0 ) mslevel=1;
                        int scanpolarity=0; 
                        if (polarity.empty() && fields.size()>7) polarity=fields[7];
                        if (!polarity.empty() && polarity[0] == '+' ) scanpolarity=1; 
                        if (!polarity.empty() && polarity[0] == '-' ) scanpolarity=-1; 
                        scan = new Scan(this,newscannum,mslevel,rt/60,precursorMz,scanpolarity);
                        if (mslevel > 1 ) scan->productMz=mz;

                        addScan(scan);
                        if (fields.size() > 7) scan->filterLine=fields[7];		//last field is srmId
                    }

                    scan->mz.push_back(mz);
                    scan->intensity.push_back(intensity);
                    lastScanNum = scannum;
                }
            }
}


void mzSample::writeMzCSV(const char* filename) {
       ofstream mzCSV;
       mzCSV.open(filename);
	   if (!mzCSV.is_open()) { cerr << "Unable to write to a file" << filename; return; }
		 
       mzCSV << "scannum,rt,mz,intensity,mslevel,precursorMz,polarity,srmid" << endl;
       for(unsigned int i=0;i<scans.size();i++ ) {
           Scan* scan = scans[i];
           for(unsigned int j=0; j< scan->nobs(); j++ ) {
               mzCSV << scan->scannum+1 << ","
               << scan->rt*60   << ","
               << scan->mz[j]   << ","
               << scan->intensity[j]   << ","
               << scan->mslevel   << ","
               << scan->precursorMz   << ","
               << (scan->getPolarity() > 0 ? "+" : "-" ) << ","
               << scan->filterLine << endl;
           }
       }
}

void mzSample::parseMzML(const char* filename) { 
    xml_document doc;

    const unsigned int parse_options = parse_minimal;

    bool loadok = doc.load_file(filename, parse_options);
    if (!loadok ) {
        cerr << "Failed to load " << filename << endl;
        return;
    }

    //Get a spectrumstore node
    xml_node chromatogramList = doc.first_child().first_element_by_path("mzML/run/chromatogramList");
    xml_node spectrumList = doc.first_child().first_element_by_path("mzML/run/spectrumList");

    if (!spectrumList.empty()) { parseMzMLSpectrumList(spectrumList); }
    else if (!chromatogramList.empty()) { parseMzMLChromatogromList(chromatogramList); }
}


void mzSample::parseMzMLChromatogromList(xml_node chromatogramList) {

    int scannum=0;
    for (xml_node chromatogram  = chromatogramList.child("chromatogram");
                                        chromatogram; chromatogram = chromatogram.next_sibling("chromatogram")) {

                        string chromatogramId = chromatogram.attribute("id").value();
                        vector<float> timeVector;
                        vector<float> intsVector;

                        xml_node binaryDataArrayList = chromatogram.child("binaryDataArrayList");
                        string precursorMzStr = chromatogram.first_element_by_path("precursor/isolationWindow/cvParam").attribute("value").value();
                        string productMzStr = chromatogram.first_element_by_path("product/isolationWindow/cvParam").attribute("value").value();
                        float precursorMz = string2float(precursorMzStr);
            float productMz = string2float(productMzStr);
            int mslevel=2;

        for( xml_node binaryDataArray= binaryDataArrayList.child("binaryDataArray");
                                binaryDataArray; binaryDataArray=binaryDataArray.next_sibling("binaryDataArray")) {
                                map<string,string>attr = mzML_cvParams(binaryDataArray);

                                int precision = 64;
                                if(attr.count("32-bit float")) precision=32;

                                string binaryDataStr = binaryDataArray.child("binary").child_value();
                                vector<float>binaryData = base64::decode_base64(binaryDataStr,precision/8,false);

                                if(attr.count("time array")) { timeVector = binaryData; }
                                if(attr.count("intensity array")) { intsVector = binaryData; }


        }

                cerr << chromatogramId << endl;
                cerr << timeVector.size()  << " ints=" << intsVector.size() << endl;
                cerr << "pre: " << precursorMz  << " prod=" << productMz << endl;

                if (precursorMz and precursorMz ) {
                                for(unsigned int i=0; i < timeVector.size(); i++ ) {
                                                Scan* scan = new Scan(this,scannum++,mslevel,timeVector[i],precursorMz,-1);
                                                scan->productMz=productMz;
                                                scan->mz.push_back(productMz);
                                                scan->filterLine= chromatogramId;
                                                scan->intensity.push_back(intsVector[i]);
                                                addScan(scan);
                                }
                }
    }

        //renumber scans based on retention time
        std::sort(scans.begin(), scans.end(),Scan::compRt);
        for(unsigned int i=0; i< scans.size(); i++ ) {
                        scans[i]->scannum=i+1;
        }

}

void mzSample::parseMzMLSpectrumList(xml_node spectrumList) {

    //Iterate through spectrums
    int scannum=0;

    for (xml_node spectrum  = spectrumList.child("spectrum");
            spectrum; spectrum = spectrum.next_sibling("spectrum")) {
        string spectrumId = spectrum.attribute("id").value();
        cerr << "Processing: " << spectrumId << endl;

        if (spectrum.empty()) continue;
        map<string,string>cvParams = mzML_cvParams(spectrum);


         int mslevel=1;
            int scanpolarity=0;
        float rt=0;
        vector<float> mzVector;
        vector<float> intsVector;

        if(cvParams.count("ms level")) {
            string msLevelStr = cvParams["ms level"];
            mslevel=(int) string2float(msLevelStr);
        }

        if(cvParams.count("positive scan")) scanpolarity=1;
        else if(cvParams.count("negative scan")) scanpolarity=-1;
        else scanpolarity=0;

        xml_node scanNode = spectrum.first_element_by_path("scanList/scan");
        map<string,string>scanAttr = mzML_cvParams(scanNode);
        if(scanAttr.count("scan start time")) {
            string rtStr = scanAttr["scan start time"];
            rt = string2float(rtStr);
        }


        map<string,string>isolationWindow = mzML_cvParams(spectrum.first_element_by_path("precursorList/precursor/isolationWindow"));
        string precursorMzStr = isolationWindow["isolation window target m/z"];
        float precursorMz = 0; if(string2float(precursorMzStr)>0) precursorMz=string2float(precursorMzStr);

        string productMzStr = spectrum.first_element_by_path("product/isolationWindow/cvParam").attribute("value").value();
        float productMz = 0;   if(string2float(productMzStr)>0)   productMz=string2float(productMzStr);

        xml_node binaryDataArrayList = spectrum.child("binaryDataArrayList");
        if( ! binaryDataArrayList or binaryDataArrayList.empty()) continue;

        for(xml_node binaryDataArray= binaryDataArrayList.child("binaryDataArray");
                binaryDataArray; binaryDataArray=binaryDataArray.next_sibling("binaryDataArray")) {
                if( ! binaryDataArray or binaryDataArray.empty()) continue;

                map<string,string>attr = mzML_cvParams(binaryDataArray);

            int precision = 64;
            if(attr.count("32-bit float")) precision=32;

            string binaryDataStr = binaryDataArray.child("binary").child_value();
            if (!binaryDataStr.empty()) {
                vector<float>binaryData = base64::decode_base64(binaryDataStr,precision/8,false);
                if(attr.count("m/z array")) { mzVector = binaryData; }
                if(attr.count("intensity array")) { intsVector = binaryData; }
            }
        }

        cerr << " scan=" << scannum << "\tms="<< mslevel << "\tprecMz" << precursorMz << "\t rt=" << rt << endl;
        Scan* scan = new Scan(this,scannum++,mslevel,rt,precursorMz,scanpolarity);
        scan->productMz=productMz;
        scan->filterLine= spectrumId;
        scan->intensity = intsVector;
        scan->mz= mzVector;
        addScan(scan);
    }
 }

map<string,string> mzSample::mzML_cvParams(xml_node node) { 
		map<string,string>attr;
        if(!node || node.empty()) return attr;
    	for (xml_node cv = node.child("cvParam"); cv; cv = cv.next_sibling("cvParam")) {
				string name = cv.attribute("name").value();
				string value = cv.attribute("value").value();
				attr[name]=value;
				//cerr << name << "->" << value << endl;
		}
		return(attr);
}



void mzSample::parseMzData(const char* filename) { 
    xml_document doc;

    const unsigned int parse_options = parse_minimal;

    bool loadok = doc.load_file(filename, parse_options);
    if (!loadok ) {
        cerr << "Failed to load " << filename << endl;
        return;
    }

    //Get a spectrumstore node
    xml_node spectrumstore = doc.first_child().child("spectrumList");

    //Iterate through spectrums
    int scannum=0;

    for (xml_node spectrum = spectrumstore.child("spectrum"); spectrum; spectrum = spectrum.next_sibling("spectrum")) {

        scannum++;
        float rt = 0;
        float precursorMz = 0;
        char scanpolarity = 0;	//default case

        xml_node spectrumInstrument = spectrum.first_element_by_path("spectrumDesc/spectrumSettings/spectrumInstrument");
        int mslevel =  spectrumInstrument.attribute("msLevel").as_int();
        //cerr << mslevel << " " << spectrum.attribute("msLevel").value() << endl;

        for( xml_node cvParam= spectrumInstrument.child("cvParam"); cvParam; cvParam= cvParam.next_sibling("cvParam")) {
            //	cout << "cvParam=" << cvParam.attribute("name").value() << endl;
            //
            if (strncasecmp(cvParam.attribute("name").value(),"TimeInMinutes",10) == 0 ) {
                rt=cvParam.attribute("value").as_float();
                //cout << "rt=" << rt << endl;
            }

            if (strncasecmp(cvParam.attribute("name").value(),"time in seconds",10) == 0 ) {
                rt=cvParam.attribute("value").as_float()/60;
                //cout << "rt=" << rt << endl;
            }


            if (strncasecmp(cvParam.attribute("name").value(),"polarity",5) == 0 ) {
                if ( cvParam.attribute("value").value()[0] == 'p' || cvParam.attribute("value").value()[0] == 'P') {
                    scanpolarity = +1;
                } else {
                    scanpolarity = -1;
                }
            }
        }

        //cout << spectrum.first_element_by_path("spectrumDesc/spectrumSettings/spectrumInstrument").child_value() << endl
        if (mslevel <= 0 ) mslevel=1;
        Scan* scan = new Scan(this,scannum,mslevel,rt,precursorMz,scanpolarity);
        addScan(scan);

        int precision1 = spectrum.child("intenArrayBinary").child("data").attribute("precision").as_int();
        string b64intensity = spectrum.child("intenArrayBinary").child("data").child_value();
        scan->intensity = base64::decode_base64(b64intensity,precision1/8,false);

        //cout << "mz" << endl;
        int precision2 = spectrum.child("mzArrayBinary").child("data").attribute("precision").as_int();
        string b64mz = spectrum.child("mzArrayBinary").child("data").child_value();
        scan->mz = base64::decode_base64(b64mz,precision2/8,false);

        //cout << "spectrum " << spectrum.attribute("title").value() << endl;
    }
}

void mzSample::parseMzXML(const char* filename) { 
	    xml_document doc;
            try {

                bool loadok = doc.load_file(filename,pugi::parse_minimal);

                if (!loadok ) {
                    cerr << "Failed to load " << filename << endl;
                    return;
                }

                //Get a spectrumstore node
                xml_node spectrumstore = doc.first_child().child("msRun");
                if (spectrumstore.empty()) {
                    xml_node scan = doc.first_child().child("scan");
                    if(!scan.empty()) { spectrumstore=doc.first_child(); }  
                    else { cerr << "parseMzXML: can't find <msRun> or <scan> section" << endl; return; }
                }

                xml_node msInstrument = spectrumstore.child("msInstrument");
                if (!msInstrument.empty()) {
                    xml_node msManufacturer = msInstrument.child("msManufacturer");
                    xml_node msModel = msInstrument.child("msModel");
                    xml_node msIonisation = msInstrument.child("msIonisation");
                    xml_node msMassAnalyzer = msInstrument.child("msMassAnalyzer");
                    xml_node msDetector = msInstrument.child("msDetector");
                    instrumentInfo[ "msManufacturer" ] =  msManufacturer.attribute("value").value();
                    instrumentInfo[ "msModel" ] =  msModel.attribute("value").value();
                    instrumentInfo[ "msIonisation" ] =  msIonisation.attribute("value").value();
                    instrumentInfo[ "msMassAnalyzer" ] =  msMassAnalyzer.attribute("value").value();
                    instrumentInfo[ "msDetector" ] =  msDetector.attribute("value").value();
                }


                //Iterate through spectrums
                int scannum=0;

                for (xml_node scan = spectrumstore.child("scan"); scan; scan = scan.next_sibling("scan")) {
                    scannum++;
                    if (strncasecmp(scan.name(),"scan",4) == 0) {
                        parseMzXMLScan(scan,scannum);
                    }

                    for ( xml_node child = scan.first_child(); child; child = child.next_sibling()) {
                        scannum++;
                        if (strncasecmp(child.name(),"scan",4) == 0) {
                            parseMzXMLScan(child,scannum);
                        }
                    }
                }
            } catch(char* err) {
                cerr << "Failed to load file: " <<  filename << " " << err << endl;
            }
}

void mzSample::parseMzXMLScan(const xml_node& scan, int scannum) { 

    float rt = 0;
    float precursorMz = 0;
    float productMz=0;
    float collisionEnergy=0;
    int scanpolarity = 0;	//default case
    int msLevel=1;
    bool networkorder = false;
    string filterLine;
    string scanType;

    for(xml_attribute attr = scan.first_attribute(); attr; attr=attr.next_attribute()) {
        if (strncasecmp(attr.name(), "retentionTime",10) == 0 ) {
            if (strncasecmp(attr.value(),"PT",2) == 0 ) {
                rt=string2float( string(attr.value()+2));
            } else if (strncasecmp(attr.value(),"P",1) == 0 ) {
                rt=string2float( string(attr.value()+1));
            } else {
                rt=string2float( attr.value());
            }
            rt /=60;
        }

        if (strncasecmp(attr.name(),"polarity",5) == 0 ) {
            char p = attr.value()[0];
            switch(p) {
            case '+': scanpolarity=  1; break;
            case '-': scanpolarity= -1; break;
            default:
                //cerr << "Warning:: scan has unknown polarity type=" << scanpolarity << endl;
                scanpolarity=0;
                break;
            }
        }

        if (strncasecmp(attr.name(),"filterLine",9) == 0) filterLine = attr.value();
        if (strncasecmp(attr.name(),"mslevel",5) == 0 ) msLevel = string2integer(attr.value());
        if (strncasecmp(attr.name(),"basePeakMz",9) == 0 ) productMz = string2float(attr.value());
        if (strncasecmp(attr.name(),"collisionEnergy",12) == 0 ) collisionEnergy= string2float(attr.value());
        if (strncasecmp(attr.name(),"scanType",8) == 0 ) scanType = attr.value();

    }

    //work around .. get polarity from filterline
    if (scanpolarity == 0 && filterLine.size()>13 ) {
        if ( filterLine[12] == '+' ) {
            scanpolarity = 1;
        }  else {
            scanpolarity = -1;
        }
    }

    precursorMz = string2float(string(scan.child_value("precursorMz")));
    //cout << "precursorMz=" << precursorMz << endl;

    xml_node peaks =  scan.child("peaks");
    if ( ! peaks.empty() ) {
        string b64String(peaks.child_value());
        if ( b64String.empty()) return;  //no m/z intensity values


#ifdef ZLIB
        //decompress
        if(strncasecmp(peaks.attribute("compressionType").value(),"zlib",4) == 0) {
            /*
            const char* sampleEncoded32LittleZlib_ = "eJzjYfBwZgACGSitA6VtoLSTtCeYDoLSSVC6CEo3AenL2skuk4D0PI95LouA9CrnuS6bgPRxoxCXQ1B1l6D0IyjNADWfgWEakG6wZ2A4AaQVHBkYfgHpE04MDGIuDAxVLgB7LB3q";
            const char* sampleEncoded32BigZlib_ = "eJxz9mDgYQACZw8GGSitA6VtwLSntBOUDoLSSVC6CEo3uSRrXwbSk1zmecwD0otc5jqvAtKbXEKMjgPpQ1B1l6D0I6j5YOA8jYHBvgFIn2BgcFQA0r8YGJyAbBcxIK5iYAAAfLkd6g==";
            const char* sampleEncoded64LittleZlib_ = "eJz7Lz63i5Eh04EBClora9uYkfhLtOOaWJH48Xr2dexI/HXsZqYZygj+6j07zLKQ+HMOW1vkIPGbZPZb5iHxo447WRdA+AlVqT0OqkeO2RRB+AeET052eCrkbVcC4TuUrpjsMGPzefsyCP9BRFuXg/2yEMcKJPNuv7rpVIXEz2yMd6lB4kNFUfkHiqD8D/YQuhLKV4HQDvVQfiSEPtAEFe93AACmsz1+";
            const char* sampleEncoded64BigZlib_ = "eJxzyGRg7Jor/p8BChwyGZjbaitbkfisTXHaS5D47HX2evEIvnKGqRn7OiR+ltmOPauR+DkW1ofnIPHzLPfLNCHxC6ydjkc59KRWJUD4RTbHjqg6TD4pfADCL7HzFnrqMHlFqQOEX2Z/fvMMh662iAcQfoVjyDJ7JPOqnG6+uo3Er3GJb8xEcj8KcCg6AKbtP0D5lVBaBUrXO0DoSCi/CaLeoR8iDgC0Qj1+";

            string testStr(sampleEncoded64LittleZlib_);

            string uncompressed;
            //gzipInflate( testStr, uncompressed );
            uncompressed = mzUtils::decompress_string(testStr);
            //b64String = uncompressed;

             cerr << "Uncompressed : " << uncompressed << endl;
            */
        }
#endif

        if (msLevel <= 0 ) msLevel=1;
        Scan* _scan = new Scan(this,scannum,msLevel,rt,precursorMz,scanpolarity);

        if (!filterLine.empty()) _scan->filterLine=filterLine;
        if (!scanType.empty())   _scan->scanType= scanType;
        _scan->productMz=productMz;
        _scan->collisionEnergy=collisionEnergy;

        if (peaks.attribute("byteOrder").empty() || strncasecmp(peaks.attribute("byteOrder").value(),"network",5) == 0 ) {
            networkorder = true;
        }

        int precision=32;
        if (!peaks.attribute("precision").empty()) { precision = peaks.attribute("precision").as_int(); }

       // cerr << "new scan=" << scannum << " msL=" << msLevel << " rt=" << rt << " precMz=" << precursorMz << " polar=" << scanpolarity
       //    << " prec=" << precision << endl;

        vector<float> mzint = base64::decode_base64(b64String,precision/8,networkorder);
        int size = mzint.size()/2;


        _scan->mz.resize(size);
        _scan->intensity.resize(size);

       // cerr << "Network:" << networkorder << " precision" << precision <<  " size=" << size << endl;

        int j=0; int count=0;
        for(int i=0; i < size; i++ ) {
            float mzValue = mzint[j++];
            float intensityValue = mzint[j++];
            //cerr << mzValue << " " << intensityValue << endl;
            if (mzValue > 0 && intensityValue > 0 ) {
                _scan->mz[i]= mzValue;
                _scan->intensity[i] = intensityValue;
                count++;
            }
        }


        _scan->mz.resize(count);
        _scan->intensity.resize(count);

        if (filterLine.empty() && precursorMz > 0 ) {
            _scan->filterLine = scanType + ":" + float2string(_scan->precursorMz,4) + " [" + float2string(_scan->productMz,4) + "]";
        }

        //cerr << _scan->scanType << " " << _scan->precursorMz << " " << _scan->productMz;
        //cerr << " addScan:" << _scan->filterLine << endl;
        addScan(_scan);
    }
}

void mzSample::summary() { 
		cerr << "Num of obs:" << this->scans.size() << endl;
		cerr << "Rt range:" << this->minRt  << " " << this->maxRt << endl;
		cerr << "Mz range:" << this->minMz  << " " << this->maxMz << endl;
}

void mzSample::calculateMzRtRange() { 

		if (scans.size() == 0 ) {
			cerr << "sample has no data" << endl;
			return;
		}

		minRt = scans[0]->rt;
		maxRt = scans[scans.size()-1]->rt;
		minMz =  FLT_MAX;
		maxMz =  0;
		minIntensity = FLT_MAX;
		maxIntensity = 0;
		totalIntensity = 0;
		int nobs = 0;

		for (unsigned int j=0; j < scans.size(); j++ ) {
			for (unsigned int i=0; i < scans[j]->mz.size(); i++ ) {
				totalIntensity +=  scans[j]->intensity[i];
				float mz = scans[j]->mz[i]; 
				if( mz < minMz && mz > 0   ) minMz = mz; //sanity check must be greater > 0
				if (mz > maxMz && mz < 1e9 ) maxMz = mz; //sanity check m/z over a billion
				if (scans[j]->intensity[i] < minIntensity ) minIntensity = scans[j]->intensity[i];
				if (scans[j]->intensity[i] > maxIntensity ) maxIntensity = scans[j]->intensity[i];
				nobs++;
			}
		}
		//sanity check
		if (minRt <= 0 ) minRt = 0;
		if (maxRt >= 1e4 ) maxRt = 1e4;
		cerr << "calculateMzRtRange() rt=" << minRt << "-" << maxRt << " mz=" << minMz << "-" << maxMz << endl;
}

mzSlice mzSample::getMinMaxDimentions(const vector<mzSample*>& samples) {
    mzSlice d;
    d.rtmin=0;
    d.rtmax=0;
    d.mzmin=0;
    d.mzmax=0;

	if ( samples.size() > 0 ) {

			d.rtmin = samples[0]->minRt;
			d.rtmax = samples[0]->maxRt;
			d.mzmin = samples[0]->minMz;
			d.mzmax = samples[0]->maxMz;

			for(unsigned int i=1; i < samples.size(); i++) {
					if ( samples[i]->minRt < d.rtmin ) d.rtmin=samples[i]->minRt;
					if ( samples[i]->maxRt > d.rtmax ) d.rtmax=samples[i]->maxRt;
					if ( samples[i]->minMz < d.mzmin ) d.mzmin=samples[i]->minMz;
					if ( samples[i]->maxMz > d.mzmax ) d.mzmax=samples[i]->maxMz;
			}
	}

	cerr << "getMinMaxDimentions() " << d.rtmin << " " << d.rtmax << " " << d.mzmin << " " << d.mzmax << endl;

	return d;
}

float mzSample::getMaxRt(const vector<mzSample*>&samples) { 
		float maxRt=0;
		for(unsigned int i=0; i < samples.size(); i++ ) 
				if (samples[i]->maxRt > maxRt) 
						maxRt=samples[i]->maxRt; 

		return maxRt;
}

float mzSample::getAverageFullScanTime() {
	float s=0;
	int n=0;
	Scan* lscan = NULL;
	Scan* tscan = NULL;
	if ( scans.size() == 0 ) return 0;

	for(unsigned int i=1; i < scans.size(); i++ ) {
		if ( scans[i]->mslevel == 1 ) {
				tscan = scans[i];
				if ( lscan ) { s += tscan->rt-lscan->rt; n++; }
				lscan = tscan;
		}
	}
	if ( n > 0 ) return s/n;
	return 0;
}



void mzSample::enumerateSRMScans() {
    srmScans.clear();
    for(unsigned int i=0; i < scans.size(); i++ ) {
        if (scans[i]->filterLine.length()>0) {
            srmScans[scans[i]->filterLine].push_back(i);
        }
    }
    cerr << "enumerateSRMScans: " << srmScans.size() << endl;
}

Scan* mzSample::getScan(unsigned int scanNum) {
	if ( scanNum >= scans.size() ) scanNum = scans.size()-1;
	if ( scanNum < scans.size() ) {
		return(scans[scanNum]);
	} else {
		cerr << "Warning bad scan number " << scanNum << endl;
		return NULL;
	}
}

EIC* mzSample::getEIC(float precursorMz, float collisionEnergy, float productMz, float amuQ1=0.1, float amuQ2=0.5) {
    EIC* e = new EIC();
    e->sampleName = sampleName;
    e->sample = this;
    e->totalIntensity=0;
    e->maxIntensity = 0;
    e->mzmin = 0;
    e->mzmax = 0;

    for(unsigned int i=0; i < scans.size(); i++ ) {
        Scan* scan = scans[i];
        if (scan->mslevel < 2) continue;
        if (precursorMz && abs(scan->precursorMz-precursorMz)>amuQ1 ) continue;
        if (productMz && abs(scan->productMz-productMz)>amuQ2) continue;
        //if (collisionEnergy && abs(scan->collisionEnergy-collisionEnergy) > 0.5) continue;

        float maxMz=0;
        float maxIntensity=0;
        for(unsigned int k=0; k < scan->nobs(); k++ ) {
            if (scan->intensity[k] > maxIntensity ) {
                maxIntensity=scan->intensity[k];
                maxMz = scan->mz[k];
            }
        }

        e->scannum.push_back(scan->scannum);
        e->rt.push_back( scan->rt );
        e->intensity.push_back(maxIntensity);
        e->mz.push_back(maxMz);
        e->totalIntensity += maxIntensity;
        if (maxIntensity>e->maxIntensity) e->maxIntensity = maxIntensity;
    }

    if ( e->rt.size() > 0 ) {
        e->rtmin = e->rt[0];
        e->rtmax = e->rt[ e->size()-1];
    }

    float scale = getNormalizationConstant();
    if(scale != 1.0) for (unsigned int j=0; j < e->size(); j++) { e->intensity[j] *= scale; }

    if(e->size() == 0) cerr << "getEIC(Q1,CE,Q3): is empty" << precursorMz << " " << collisionEnergy << " " << productMz << endl;
    std::cerr << "getEIC(Q1,CE,Q3): srm"  << precursorMz << " " << e->intensity.size() << endl;
    return e;
}



EIC* mzSample::getEIC(string srm) {


        EIC* e = new EIC();
	e->sampleName = sampleName;
	e->sample = this;
        e->totalIntensity=0;
        e->maxIntensity = 0;
	e->mzmin = 0;
	e->mzmax = 0;

	if (srmScans.size() == 0 ) enumerateSRMScans();

	if (srmScans.count(srm) > 0 ) {
            vector<int> srmscans = srmScans[srm];
            for (unsigned int i=0; i < srmscans.size(); i++ ) {
                Scan* scan = scans[srmscans[i]];
                float maxMz=0;
                float maxIntensity=0;
                for(unsigned int k=0; k < scan->nobs(); k++ ) {
                    if (scan->intensity[k] > maxIntensity ) {
                        maxIntensity=scan->intensity[k];
                        maxMz = scan->mz[k];
                    }
                }
                e->scannum.push_back(scan->scannum);
                e->rt.push_back( scan->rt );
                e->intensity.push_back(maxIntensity);
                e->mz.push_back(maxMz);
                e->totalIntensity += maxIntensity;

                if (maxIntensity>e->maxIntensity) e->maxIntensity = maxIntensity;
            }
	}

	if ( e->rt.size() > 0 ) { 
                e->rtmin = e->rt[0];
                e->rtmax = e->rt[ e->size()-1];
	}

	float scale = getNormalizationConstant();
	if(scale != 1.0) for (unsigned int j=0; j < e->size(); j++) { e->intensity[j] *= scale; }
	if(e->size() == 0) cerr << "getEIC(SRM STRING): is empty" << srm << endl;
         std::cerr << "getEIC: srm"  << srm << " " << e->intensity.size() << endl;

	return e;
}


EIC* mzSample::getEIC(float mzmin,float mzmax, float rtmin, float rtmax, int mslevel) { 

    //ajust EIC retention time window to match sample retentention times
	if (rtmin < this->minRt ) rtmin = this->minRt;
	if (rtmax > this->maxRt ) rtmax = this->maxRt;
	if (mzmin < this->minMz ) mzmin = this->minMz;
	if (mzmax > this->maxMz ) mzmax = this->maxMz;

   //cerr << "getEIC()" << setprecision(7) << mzmin << " " << mzmax << " " << rtmin << " " << rtmax << endl;

	EIC* e = new EIC();
	e->sampleName = sampleName;
	e->sample = this;
        e->mzmin = mzmin;
        e->mzmax = mzmax;
        e->totalIntensity=0;
        e->maxIntensity = 0;

	int scanCount = scans.size();
	if ( scanCount == 0 ) return e;

	if ( mzmin < minMz && mzmax < maxMz ) {
			cerr << "getEIC(): mzmin and mzmax are out of range" << endl;
			return e;
	}
    
	//binary search mz  domain iterator
	vector<float>::iterator mzItr;

        //binary search rt domain iterator
        Scan tmpScan(this,0,1,rtmin-0.1,0,-1);
	deque<Scan*>::iterator scanItr = lower_bound(scans.begin(), scans.end(),&tmpScan, Scan::compRt);
        if (scanItr >= scans.end() ) { return e; }

	//preallocated memory for arrays [ this should really be corrected for mslevel type ]
        int estimatedScans=scans.size();

        if (this->maxRt-this->minRt > 0 && (rtmax-rtmin)/(this->maxRt-this->minRt) <= 1 ) {
            estimatedScans=float (rtmax-rtmin)/(this->maxRt-this->minRt)*scans.size()+10;
        }

	e->scannum.reserve(estimatedScans);
	e->rt.reserve(estimatedScans);
	e->intensity.reserve(estimatedScans);
	e->mz.reserve(estimatedScans);

	int scanNum=scanItr-scans.begin()-1;
	for(; scanItr != scans.end(); scanItr++ ) {
            Scan* scan = *(scanItr);
            scanNum++;
            if (scan->mslevel != mslevel) continue;
            if (scan->rt < rtmin) continue;
            if (scan->rt > rtmax) break;

            float __maxMz=0;
            float __maxIntensity=0;

            //binary search
            mzItr = lower_bound(scan->mz.begin(), scan->mz.end(), mzmin);
            int lb = mzItr-scan->mz.begin();

            for(unsigned int k=lb; k < scan->nobs(); k++ ) {
                if (scan->mz[k] < mzmin) continue;
                if (scan->mz[k] > mzmax) break;
                if (scan->intensity[k] > __maxIntensity ) {
                    __maxIntensity=scan->intensity[k];
                    __maxMz = scan->mz[k];
                }
            }

            e->scannum.push_back(scanNum);
            e->rt.push_back(scan->rt);
            e->intensity.push_back(__maxIntensity);
            e->mz.push_back(__maxMz);
            e->totalIntensity += __maxIntensity;
            if (__maxIntensity>e->maxIntensity) e->maxIntensity = __maxIntensity;

	}

	//cerr << "estimatedScans=" << estimatedScans << " actul=" << e->scannum.size() << endl;
	if ( e->rt.size() > 0 ) { 
            e->rtmin = e->rt[0];
            e->rtmax = e->rt[ e->size()-1];
            //cerr << "getEIC()" << e->scannum[0] << " " << e->scannum[e->scannum.size()-1] << " " << scans.size() << endl;
	}

	//scale EIC by normalization constant
	float scale = getNormalizationConstant();
	if(scale != 1.0) for (unsigned int j=0; j < e->size(); j++) { e->intensity[j] *= scale; }
	
	//cerr << "getEIC: maxIntensity=" << e->maxIntensity << endl;
	//e->summary();

	if(e->size() == 0) cerr << "getEIC(mzrange,rtrange,mslevel): is empty" << mzmin << " " << mzmax << " " << rtmin << " " << rtmax << endl;

	return(e);
    }


EIC* mzSample::getTIC(float rtmin, float rtmax, int mslevel) { 

    //ajust EIC retention time window to match sample retentention times
    if (rtmin < this->minRt ) rtmin =this->minRt;
    if (rtmax > this->maxRt ) rtmax =this->maxRt;

    //cerr << "getEIC()" << setprecision(7) << mzmin << " " << mzmax << " " << rtmin << " " << rtmax << endl;

    EIC* e = new EIC();
    e->sampleName = sampleName;
    e->sample = this;
    e->mzmin = 0;
    e->mzmax = 0;
    e->totalIntensity=0;
    e->maxIntensity = 0;

    int scanCount = scans.size();
    if ( scanCount == 0 ) return e;

    for(int i=0;  i < scanCount; i++)
    {
        if (scans[i]->mslevel == mslevel) {
            Scan* scan = scans[i];
            float y = scan->totalIntensity();
            e->mz.push_back(0);
            e->scannum.push_back(i);
            e->rt.push_back(scan->rt);
            e->intensity.push_back(y);
            e->totalIntensity += y;
            if (y>e->maxIntensity) e->maxIntensity=y;
        }
    }
    if ( e->rt.size() > 0 ) {
        e->rtmin = e->rt[0];
        e->rtmax = e->rt[ e->size()-1];
    }
    return(e);
}

//compute correlation between two mzs within some retention time window
float mzSample::correlation(float mz1,  float mz2, float ppm, float rt1, float rt2 ) { 
    
    float ppm1 = ppm*mz1/1e6;
    float ppm2 = ppm*mz2/1e6;
    int mslevel=1;
    EIC* e1 = mzSample::getEIC(mz1-ppm1, mz1+ppm1, rt1, rt2, mslevel);
    EIC* e2 = mzSample::getEIC(mz2-ppm2, mz2+ppm1, rt1, rt2, mslevel);
    return mzUtils::correlation(e1->intensity, e2->intensity);
}


int mzSample::parseCDF (const char* filename, int is_verbose)
{
    #ifdef CDFPARSER
    int cdf=0;
    int errflag = 0;
    long nscans=0;
    long ninst=0;

    extern int ncopts;              /* from "netcdf.h" */
    ncopts = 0;

    static MS_Admin_Data            admin_data;
    static MS_Sample_Data           sample_data;
    static MS_Test_Data             test_data;
    static MS_Instrument_Data       inst_data;
    static MS_Raw_Data_Global       raw_global_data;
    static MS_Raw_Per_Scan          raw_data;
    double  mass_pt=0;
    double inty_pt=0;
    double inty=0;

    cdf = ms_open_read( filename );
    if ( -1 == cdf )
    {
        fprintf( stderr, "\nopen_cdf_ms: ms_open_read failed!" );
        return 0;
    }

    /* Initialize attribute data structures */

    ms_init_global( FALSE, &admin_data, &sample_data, &test_data, &raw_global_data );


    /* Read global information */

    if (MS_ERROR == ms_read_global( cdf, &admin_data, &sample_data, &test_data, &raw_global_data))
    {
        fprintf( stderr, "\nopen_cdf_ms: ms_read_global failed!" );
        ms_init_global( TRUE, &admin_data, &sample_data, &test_data, &raw_global_data);
        ms_close(cdf);
        return 0;
    }

    nscans = raw_global_data.nscans;

    switch (admin_data.experiment_type)
    {
    case 0:
        printf ("Centroid");
        break;
    case 1:
        printf ("Continuum");
        break;
    case 2:
        printf ("Library");
        break;
    default:
        printf ("Unknown: '%d'", admin_data.experiment_type);
        break;
    }

    printf ("\n\n-- Instrument Information --");
    ninst = admin_data.number_instrument_components;
    printf ("\nNumber_inst_comp\t%ld", ninst);


    printf ("\n\n-- Raw Data Information --");
    printf ("\nNumber of scans\t\t%ld", nscans);

    printf ("\nMass Range\t\t%.2f > %.2f",
            raw_global_data.mass_axis_global_min,
            raw_global_data.mass_axis_global_max);

    printf ("\nInty Range\t\t%.2f > %.2f",
            raw_global_data.intensity_axis_global_min,
            raw_global_data.intensity_axis_global_max);

    printf ("\nTime Range\t\t%.2f > %.2f",
            raw_global_data.time_axis_global_min,
            raw_global_data.time_axis_global_max);

    printf ("\nActual Run Time\t\t%.2f (%.2f min)",
            raw_global_data.run_time, raw_global_data.run_time/60.0);
    printf ("\nComments \t\t%s", raw_global_data.comments);


    if (errflag)                    /* if error occurred, clean up and leave */
    {
        ms_init_global( TRUE, &admin_data, &sample_data, &test_data, &raw_global_data );
        ms_close( cdf );
        return 0;
    }

    /* Check to see if scale factors and offsets are set to "NULL"
        values; if so, correct them for use below */

    if ((int)MS_NULL_FLT == (int)raw_global_data.mass_factor)
        raw_global_data.mass_factor = 1.0;

    if ((int)MS_NULL_FLT == (int)raw_global_data.time_factor)
        raw_global_data.time_factor = 1.0;

    if ((int)MS_NULL_FLT == (int)raw_global_data.intensity_factor)
        raw_global_data.intensity_factor = 1.0;

    if ((int)MS_NULL_FLT == (int)raw_global_data.intensity_offset)
        raw_global_data.intensity_offset = 0.0;

    if ((raw_global_data.mass_axis_global_min < 0) || (raw_global_data.mass_axis_global_max < 0))
    {
        /* this bug is frequently observed with files from HP/Agilent ChemStation */
        fprintf (stderr, "\n*** WARNING: Negative mass reported! Use '-v' for details.\n\n");
    }

    for (int scan = 0; scan < nscans; scan++)
    {
        ms_init_per_scan(FALSE, &raw_data, NULL);
        raw_data.scan_no = (long) scan;
        mass_pt = inty_pt = inty = 0.0;                     /* init */

        if (MS_ERROR == ms_read_per_scan(cdf, &raw_data, NULL))
        {               /* free allocated memory before leaving */
            fprintf(stderr, "\nreadchro: ms_read_per_scan failed (scan %d)!", scan);
            ms_init_per_scan(TRUE, &raw_data, NULL);
            return 0;
        }

        if (!raw_data.points) {       /* empty scan? */
            break;
        } else {                       /* there are data points */

            int polarity=0;
            if ( test_data.ionization_mode == polarity_plus) polarity=+1;
            else polarity = -1;


            Scan* myscan = new Scan(this,raw_data.actual_scan_no,
                                    test_data.scan_function - (int) resolution_proportional,
                                    raw_data.scan_acq_time/60,
                                    0,
                                    polarity);


            myscan->intensity.resize(raw_data.points);
            myscan->mz.resize(raw_data.points);

            if (admin_data.experiment_type == 0)
                myscan->centroided=true;
            else
                myscan->centroided=false;

            for (int i = 0; i < raw_data.points; i++)
            {
                switch( raw_global_data.mass_format )
                {
                case data_short:
                    mass_pt = (double) ((short *)raw_data.masses)[i];
                    break;

                case data_long:
                    mass_pt = (double) ((long *)raw_data.masses)[i];
                    break;

                case data_float:
                    mass_pt = (double) ((float *)raw_data.masses)[i];
                    break;

                case data_double:
                    mass_pt = ((double *)raw_data.masses)[i];
                    break;
                }

                mass_pt *= raw_global_data.mass_factor;

                switch( raw_global_data.intensity_format )
                {
                case data_short:
                    inty_pt = (double) ((short *)raw_data.intensities)[i];
                    break;

                case data_long:
                    inty_pt = (double) ((long *)raw_data.intensities)[i];
                    break;

                case data_float:
                    inty_pt = (double) ((float *)raw_data.intensities)[i];
                    break;

                case data_double:
                    inty_pt = (double) ((double *)raw_data.intensities)[i];
                    break;
                }

                inty_pt = inty_pt * raw_global_data.intensity_factor + raw_global_data.intensity_offset;
                //cerr << "mz/int" << mass_pt << " " << inty_pt << endl;
                myscan->intensity[i]=inty_pt;
                myscan->mz[i]=mass_pt;

                if (raw_data.flags > 0) printf("\nWarning: There are flags in scan %ld (ignored).", scan);
            }       /* i loop */


            addScan(myscan);
        }


        ms_init_per_scan( TRUE, &raw_data, NULL );

    }   /* scan loop */
    #endif
    return 1;
}

Scan* mzSample::getAverageScan(float rtmin, float rtmax, int mslevel, int polarity, float sd ) {

    float rt=rtmin + (rtmax-rtmin)/2;
    int scanCount=0;
    int scannum=0;

    map<float,double> mz_intensity_map;
    map<float,double> mz_bin_map;
    map<float,int> mz_count;

    for(unsigned int s=0; s < scans.size(); s++) {
        if(scans[s]->getPolarity() != polarity
                || scans[s]->mslevel != mslevel
                || scans[s]->rt < rtmin
                || scans[s]->rt > rtmax) continue;

        Scan* scan = scans[s];
        scanCount++;
        for(unsigned int i=0; i < scan->mz.size(); i++) {
                float bin = FLOATROUND(scan->mz[i],sd);
                mz_intensity_map[bin] += ((double) scan->intensity[i]);
                mz_bin_map[bin] += ((double)(scan->intensity[i])*(scan->mz[i]));
                mz_count[bin]++;
        }
    }

    Scan* avgScan = new Scan(this,scannum,mslevel,rt/scanCount, 0, polarity);

    map<float,double>::iterator itr;
    for(itr = mz_intensity_map.begin(); itr != mz_intensity_map.end(); ++itr ) {
        float bin = (*itr).first;
        double totalIntensity=(*itr).second;
        double avgMz =  mz_bin_map[bin] / totalIntensity;
        avgScan->mz.push_back((float)avgMz);
        avgScan->intensity.push_back((float) totalIntensity / mz_count[bin]);
    }
    //cout << "getAverageScan() from:" << from << " to:" << to << " scanCount:" << scanCount << "scans. mzs=" << avgScan->nobs() << endl;
    return avgScan;
}


void mzSample::saveOriginalRetentionTimes() {
    if ( originalRetentionTimes.size() > 0 ) return;

    originalRetentionTimes.resize(scans.size());
    for(unsigned int ii=0; ii < scans.size(); ii++ ) {
        originalRetentionTimes[ii]=scans[ii]->rt;
    }
}

void mzSample::restoreOriginalRetentionTimes() {
    if ( originalRetentionTimes.size() == 0 ) return;

    for(unsigned int ii=0; ii < scans.size(); ii++ ) {
        scans[ii]->rt = originalRetentionTimes[ii];
    }
}


vector<float> mzSample::getIntensityDistribution(int mslevel) {

    vector<float> allintensities;
    for(unsigned int s=0; s < this->scans.size(); s++) {
        Scan* scan = this->scans[s];
        if (scan->mslevel != mslevel) continue;

        for(unsigned int i=0; i < scan->mz.size(); i++) {
            allintensities.push_back(scan->intensity[i]);
        }
    }

    return(quantileDistribution(allintensities));
}
