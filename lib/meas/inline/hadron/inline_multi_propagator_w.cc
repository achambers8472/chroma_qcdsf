// $Id: inline_multi_propagator_w.cc,v 2.2 2005-11-30 04:46:39 edwards Exp $
/*! \file
 * \brief Inline construction of propagator
 *
 * Propagator calculations
 */

#include "fermact.h"
#include "meas/inline/hadron/inline_multi_propagator_w.h"
#include "meas/inline/abs_inline_measurement_factory.h"
#include "meas/glue/mesplq.h"
#include "util/ft/sftmom.h"
#include "util/info/proginfo.h"
#include "actions/ferm/fermacts/fermact_factory_w.h"
#include "actions/ferm/fermacts/fermacts_aggregate_w.h"
#include "actions/ferm/fermacts/overlap_fermact_base_w.h"
#include "meas/inline/io/named_objmap.h"

#include "meas/inline/make_xml_file.h"
#include <iomanip>

namespace Chroma 
{ 
  namespace InlineMultiPropagatorEnv 
  { 
    AbsInlineMeasurement* createMeasurement(XMLReader& xml_in, 
					    const std::string& path) 
    {
      return new InlineMultiPropagator(InlineMultiPropagatorParams(xml_in, path));
    }

    bool registerAll()
    {
      bool foo = true;
      foo &= TheInlineMeasurementFactory::Instance().registerObject(name, createMeasurement);
      foo &= WilsonTypeFermActsEnv::registered;
      return foo;
    }

    const std::string name = "MULTI_PROPAGATOR";
    const bool registered = registerAll();
  };


  //! MultiPropagator input
  void read(XMLReader& xml, const string& path, InlineMultiPropagatorParams::NamedObject_t& input)
  {
    XMLReader inputtop(xml, path);

    read(inputtop, "source_id", input.source_id);
    read(inputtop, "prop_id", input.prop_id);
  }

  //! MultiPropagator output
  void write(XMLWriter& xml, const string& path, const InlineMultiPropagatorParams::NamedObject_t& input)
  {
    push(xml, path);

    write(xml, "source_id", input.source_id);
    write(xml, "prop_id", input.prop_id);

    pop(xml);
  }


  // Param stuff
  InlineMultiPropagatorParams::InlineMultiPropagatorParams() { frequency = 0; }

  InlineMultiPropagatorParams::InlineMultiPropagatorParams(XMLReader& xml_in, const std::string& path) 
  {
    try 
    {
      XMLReader paramtop(xml_in, path);

      if (paramtop.count("Frequency") == 1)
	read(paramtop, "Frequency", frequency);
      else
	frequency = 1;

      // Parameters for source construction
      read(paramtop, "Param", param);

      // Read any auxiliary state information
      if( paramtop.count("StateInfo") == 1 ) {
	XMLReader xml_state_info(paramtop, "StateInfo");
	std::ostringstream os;
	xml_state_info.print(os);
	stateInfo = os.str();
      }
      else { 
	XMLBufferWriter s_i_xml;
	push(s_i_xml, "StateInfo");
	pop(s_i_xml);
	stateInfo = s_i_xml.printCurrentContext();
      }

      // Ids
      read(paramtop, "NamedObject", named_obj);

      // Possible alternate XML file pattern
      if (paramtop.count("xml_file") != 0) 
      {
	read(paramtop, "xml_file", xml_file);
      }
    }
    catch(const std::string& e) 
    {
      QDPIO::cerr << __func__ << ": Caught Exception reading XML: " << e << endl;
      QDP_abort(1);
    }
  }


  void
  InlineMultiPropagatorParams::write(XMLWriter& xml_out, const std::string& path) 
  {
    push(xml_out, path);
    
    Chroma::write(xml_out, "Param", param);
    {
      //QDP::write(xml_out, "StateInfo", stateInfo);
      istringstream header_is(stateInfo);
      XMLReader xml_header(header_is);
      xml_out << xml_header;
    }
    Chroma::write(xml_out, "NamedObject", named_obj);

    pop(xml_out);
  }


  // Function call
  void 
  InlineMultiPropagator::operator()(const multi1d<LatticeColorMatrix>& u,
				    XMLBufferWriter& gauge_xml,
				    unsigned long update_no,
				    XMLWriter& xml_out) 
  {
    // If xml file not empty, then use alternate
    if (params.xml_file != "")
    {
      string xml_file = makeXMLFileName(params.xml_file, update_no);

      push(xml_out, "multi_propagator");
      write(xml_out, "update_no", update_no);
      write(xml_out, "xml_file", xml_file);
      pop(xml_out);

      XMLFileWriter xml(xml_file);
      func(u, gauge_xml, update_no, xml);
    }
    else
    {
      func(u, gauge_xml, update_no, xml_out);
    }
  }


  // Real work done here
  void 
  InlineMultiPropagator::func(const multi1d<LatticeColorMatrix>& u,
			 XMLBufferWriter& gauge_xml,
			 unsigned long update_no,
			 XMLWriter& xml_out) 
  {
    START_CODE();

    push(xml_out, "multi_propagator");
    write(xml_out, "update_no", update_no);

    QDPIO::cout << "MULTI_PROPAGATOR: multimass propagator calculation" << endl;

    // Write out the input
    params.write(xml_out, "Input");

    // Write out the config header
    write(xml_out, "Config_info", gauge_xml);

    push(xml_out, "Output_version");
    write(xml_out, "out_version", 1);
    pop(xml_out);

    // Calculate some gauge invariant observables just for info.
    MesPlq(xml_out, "Observables", u);

    //
    // Read in the source along with relevant information.
    // 
    XMLReader source_file_xml, source_record_xml;
    bool make_sourceP = false;
    bool seqsourceP = false;
    try
    {
      // Create the space
      TheNamedObjMap::Instance().create<LatticePropagator>(params.named_obj.source_id);
	
      // Snarf the prop info. This is will throw if the source_id is not there
      TheNamedObjMap::Instance().get(params.named_obj.source_id).getFileXML(source_file_xml);
      TheNamedObjMap::Instance().get(params.named_obj.source_id).getRecordXML(source_record_xml);

      // Try to invert this record XML into a source struct
      // First identify what kind of source might be here
      if (source_record_xml.count("/MakeSource") != 0)
      {
	PropSourceConst_t source_header;

	read(source_record_xml, "/MakeSource/PropSource", source_header);
	make_sourceP = true;
      }
      else if (source_record_xml.count("/SequentialSource") != 0)
      {
	SeqSource_t seqsource_header;

	read(source_record_xml, "/SequentialSource/SeqSource", seqsource_header);
	seqsourceP = true;
      }
      else
	throw std::string("No appropriate header found");

      // Write out the source header
      write(xml_out, "Source_file_info", source_file_xml);
      write(xml_out, "Source_record_info", source_record_xml);
    }
    catch (std::bad_cast)
    {
      QDPIO::cerr << InlineMultiPropagatorEnv::name << ": caught dynamic cast error" 
		  << endl;
      QDP_abort(1);
    }
    catch (const string& e) 
    {
      QDPIO::cerr << InlineMultiPropagatorEnv::name << ": error message: " << e << endl;
      QDP_abort(1);
    }

    // Sanity check
    if (seqsourceP) {
      QDPIO::cerr << "Sequential propagator not supportd under multi-mass " << endl;
      QDPIO::cerr << "since source is not mass independent " << endl;
      QDP_abort(1);      
    }

    proginfo(xml_out);    // Print out basic program info

    // Sanity check - write out the norm2 of the source in the Nd-1 direction
    // Use this for any possible verification
    {
      // Initialize the slow Fourier transform phases
      SftMom phases(0, true, Nd-1);

      multi1d<Double> source_corr = 
	sumMulti(localNorm2(TheNamedObjMap::Instance().getData<LatticePropagator>(params.named_obj.source_id)),
		 phases.getSet());

      push(xml_out, "Source_correlator");
      write(xml_out, "source_corr", source_corr);
      pop(xml_out);
    }

    int num_mass = params.param.MultiMasses.size();
    multi1d<LatticePropagator> quark_propagator(num_mass);
    int ncg_had = 0;

    //
    // Initialize fermion action
    //
    std::istringstream  xml_s(params.param.fermact);
    XMLReader  fermacttop(xml_s);
    const string fermact_path = "/FermionAction";
    string fermact;

    try
    {
      read(fermacttop, fermact_path + "/FermAct", fermact);
    }
    catch (const std::string& e) 
    {
      QDPIO::cerr << "Error reading fermact: " << e << endl;
      QDP_abort(1);
    }

    QDPIO::cout << "FermAct = " << fermact << endl;


    // Deal with auxiliary (and polymorphic) state information
    // eigenvectors, eigenvalues etc. The XML for this should be
    // stored as a string called "stateInfo" in the param struct.

    // Make a reader for the stateInfo
    std::istringstream state_info_is(params.stateInfo);
    XMLReader state_info_xml(state_info_is);
    string state_info_path="/StateInfo";

    //
    // Try the factories
    //
    try {
      QDPIO::cout << "Try the various factories" << endl;

      // Generic Wilson-Type stuff
      Handle< FermionAction<LatticeFermion> >
	S_f(TheFermionActionFactory::Instance().createObject(fermact,
							     fermacttop,
							     fermact_path));

      
      // If this cast fails a bad cast exception is thrown.
      OverlapFermActBase& S_ov = dynamic_cast<OverlapFermActBase&>(*S_f);
      
      Handle<const ConnectState> state(S_ov.createState(u,
							state_info_xml,
							state_info_path));  // uses phase-multiplied u-fields
      
      QDPIO::cout << "Suitable factory found: compute the quark prop" << endl;
      
      S_ov.multiQuarkProp(quark_propagator, 
			  xml_out, 
			  TheNamedObjMap::Instance().getData<LatticePropagator>(params.named_obj.source_id),
			  state, 
			  params.param.MultiMasses, 
			  params.param.invParam, 
			  1,
			  ncg_had);
    }
    catch (const std::string& e) {
      QDPIO::cout << "4D: " << e << endl;
    }
    catch(bad_cast) { 
      QDPIO::cerr << "Fermion action created cannot be used for multi mass qprop" << endl;
      QDP_abort(1);
    }

    push(xml_out,"Relaxation_Iterations");
    write(xml_out, "ncg_had", ncg_had);
    pop(xml_out);

   // Sanity check - write out the propagator (pion) correlator in the Nd-1 direction
    {
      // Initialize the slow Fourier transform phases
      SftMom phases(0, true, Nd-1);
      for(int m=0; m < num_mass; m++) { 
	multi1d<Double> prop_corr = sumMulti(localNorm2(quark_propagator[m]), 
					     phases.getSet());
	
	push(xml_out, "Prop_correlator");
	write(xml_out, "Number", m);
	write(xml_out, "Mass", params.param.MultiMasses[m]);
	write(xml_out, "prop_corr", prop_corr);
	pop(xml_out);
      }
    }
    

    // Save the propagators
    for(int m=0; m < num_mass; m++) 
    {
      try
      {
	XMLBufferWriter file_xml;
	push(file_xml, "propagator");
	int id = 0;    // NEED TO FIX THIS - SOMETHING NON-TRIVIAL NEEDED
	write(file_xml, "id", id);
	pop(file_xml);
      
	XMLBufferWriter record_xml;
	push(record_xml, "Propagator");
      
	// Jiggery pokery. Substitute the ChromaMultiProp_t with a 
	// ChromaProp. This is a pisser because of the FermActParams
	// THIS IS NOT TOTALLY KOSHER AS IT CHANGES THE MASS IN INPUT
	// PARAM as well. However, at this stage we have no further need
	// for input param.
	// I Will eventually write Copy Constructors.
      
	// ChromaProp_t out_param(input.param, m);
      
	ChromaProp_t out_param;
	out_param.nonRelProp = params.param.nonRelProp;
	out_param.fermact = params.param.fermact;
	// I need a way to glom a mass into an XML document
	//
	// something like XMLReader::replace(path, value)
	// for now all masses have the same mass on output
	// buggerisations...
	out_param.invParam.invType = params.param.invParam.invType;
	out_param.invParam.MROver = params.param.invParam.MROver;
	out_param.invParam.MaxCG = params.param.invParam.MaxCG;
	out_param.invParam.RsdCG = params.param.invParam.RsdCG[m];
//	out_param.nrow =params.param.nrow ;
      
      
	write(record_xml, "ForwardProp", out_param);
	XMLReader xml_tmp(source_record_xml, "/MakeSource");
	record_xml << xml_tmp;
	pop(record_xml);
      
	ostringstream outfile;
	outfile << params.named_obj.prop_id << "_" << setw(3) << setfill('0') << m;

	QDPIO::cout << "Attempting to save " << outfile.str() << endl;
      
	// Create the space
	TheNamedObjMap::Instance().create<LatticePropagator>(outfile.str());
	TheNamedObjMap::Instance().getData<LatticePropagator>(outfile.str()) 
	  = quark_propagator[m];

	// Write the propagator xml info
	TheNamedObjMap::Instance().get(outfile.str()).setFileXML(file_xml);
	TheNamedObjMap::Instance().get(outfile.str()).setRecordXML(record_xml);
      }
      catch (std::bad_cast)
      {
	QDPIO::cerr << InlineMultiPropagatorEnv::name << ": caught dynamic cast error" 
		    << endl;
	QDP_abort(1);
      }
      catch (const string& e) 
      {
	QDPIO::cerr << InlineMultiPropagatorEnv::name << ": error extracting prop_header: " << e << endl;
	QDP_abort(1);
      }
    }
   
    
    pop(xml_out);  // propagator
    
    QDPIO::cout << "MultiPropagator ran successfully" << endl;
    
    END_CODE();
  } 
  
};
