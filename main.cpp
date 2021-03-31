#include <getopt.h>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
#include "json_lite.h"

//
// [stuff] # starts a new filter.
// filter gets more restrictive as extra
// classes are sequentially added.
// if a filter is added in the same class as an active filter
// then that disables the earlier filter, and adds the current filter.
//
// [all]  disables all filters
// [none] selects nothing - items within [none] match nothing.
//

//////////////////////////////////////////////////
// Filter
// models the line which enables a filter.
// full line is mLine
// mClass is the class which this filter belongs to.
// mKey is the value left of the equals sign e.g. gpio4
// mValue is the value after the equals sign e.g. 1
// mLine is the line which was parsed to get this data.
class Filter
{

public:
  std::string mClass;
  std::string mKey;
  std::string mValue;
  std::string mLine;
  bool mEmpty;
  Filter()
    : mEmpty( true )
  {}
  Filter( const std::string & fltClass, const std::string & key
	  , const std::string & value, const std::string & line )
    : mClass( fltClass )
    , mKey( key )
    , mValue( value )
    , mLine( line )
    , mEmpty( false )
  {
  }
  Filter( const char *keyValue, const char * fltClass )
    : mClass( fltClass )
  {
    std::string str = keyValue;
    size_t equals = str.find( '=' );
    if( equals != std::string::npos ){
      mKey = str.substr( 0,equals );
      mValue = str.substr( equals+1);
    } else {
      mKey = keyValue;
    }
    mLine = "[" + str + "]";
  }
  static bool parseFilter( const std::string & line, std::string &key,
		      std::string &value)
  {
    // [gpio3=1] key=gpio3  value=1
    // [0x01243] key=0x1243
    // [HDMI:0]  key=HDMI:0
    if( line.length() == 0 || line[0] != '[' ){
      return false;
    }
    std::string _key;
    std::string _value;
    enum inputMode {imKey, imValue };
    inputMode mode = imKey;
    std::string::size_type pos = 1;
    while( pos < line.length() ){
      if( line[pos] == ']' ){
	if( _key.length()> 0 &&
	    (mode == imKey || _value.length() > 0 )){
	  key = _key;
	  value = _value;
	  return true;
	} else {
	  return false;
	}
      } else if( line[pos] == '=' ){
	if( mode == imValue ){
	  return false;
	}
	mode = imValue;
      } else {
	if( mode == imKey ){
	  _key += line[pos];
	} else {
	  _value += line[pos];
	}
      }
      pos++;
    }
    return false;
  }
};

class ConfigSetup;
///////////////////////////////////////////////////////////////////
// section - created each time the filter changes.
// describes the lines with a specific filter-set added.
// mSelection - the filters which are active.
// mEntryFilter - the filter which started this section.  (May be empty for first section)
// mLines - the lines in the section
class Section
{
public:
  std::vector< Filter > mSelection;  // which filters are active...
  Filter mEntryFilter;
  std::vector< std::string > mLines;
  bool sectionChange( const std::string & line, const ConfigSetup & config );
  bool matches( const std::vector< Filter> & requiredFilters )
  {
    bool failed = false;
    std::vector<Filter> copy = requiredFilters;
    if( copy.size() == 0 ){
      if( mSelection.size() == 0 ) {
	return true;
      }
      if( isAll() ){
	return true;
      }
      return false;
    }
    if( copy.size() == 1 && mSelection.size() == 0 ){
      if( copy[0].mClass == "super" && copy[0].mKey == "all" ) {
	return true;
      }
      return false;
    }
    for( auto flt = mSelection.begin(); failed == false && flt != mSelection.end(); flt++ ){
      bool found = false;
      for( auto cpFlt = copy.begin(); found == false && cpFlt != copy.end(); cpFlt++ ){
	if( cpFlt->mClass == flt->mClass &&
	    cpFlt->mKey == flt->mKey &&
	    cpFlt->mValue == flt->mValue ){
	  found = true;
	  copy.erase( cpFlt );
	  break;
	}
      }
      if( found == false ){
	failed = true;
      }
    }
    return !failed;
  }
  bool isAll()
  {
    if( mSelection.size() == 0 ) return true;
    if( mSelection.size() > 1 ) return false;
    if( mSelection[0].mClass == "super" && mSelection[0].mKey == "all" ){
      return true;
    }
    return false;
  }
};

class WholeFile
{
public:
  std::vector<Section> mSections;
  void resetToAll()
  {
    if( mSections.size() == 0 ) return;
    Section & last = mSections[ mSections.size() -1 ];
    if( last.isAll() ) return;
    Section all;
    Filter allFlt( "super", "all", "", "[all]" );
    all.mEntryFilter = allFlt;
    all.mSelection.push_back( allFlt );
  }
  void addLine( const std::string & line )
  {
    if( mSections.size() == 0 ){
      Section newSection;
      mSections.push_back(newSection);
    }
    mSections[mSections.size()-1].mLines.push_back( line );
  }
};


  
//////////////////////////////////////////////////////////////
// sigh - after reading json spec.  All "strings" are quoted with "
//
//  So this looks less nice.  However, should aid flexibility....
///
const char * config =
  R"##config( {
  "platform"  : [ "pi0", "pi0w" , "pi1", "pi2", "pi3", "pi3+", "pi4" ],
  "super" :     [ "all", "none" ],
  "edid" :      [ "edid" ],
  "cpuserial" : [ "0x%x" ], 
  "hdmi" :      [ "HDMI:0", "HDMI:1" ],
  "gpio" :      [ "gpio%d" ] 
  })##config";
class Description
{
  std::string mBase;
  std::string mParam;
public:
  Description()
  {}
  Description( const char * str )
  {
    std::string s = str;
    std::string::size_type pos = s.find( '%' );
    if( pos != std::string::npos ){
      mBase = s.substr(0,pos );
      mParam = s.substr( pos+1);
    } else {
      mBase = s;
    }
    //printf( "base = %s, pattern = %s\n", mBase.c_str(), mParam.c_str() );    
  }
  const std::string & base() const
  {
    return mBase;
  }
  const std::string &param() const
  {
    return mParam;
  }
};


class ConfigValue
{
public:
  Description mDesc;
  std::string mClass;
  ConfigValue( const char * value )
    : mDesc( value )
  {}
  ConfigValue()
  {}
  bool isValid( const std::string & key ) const
  {
    if( mDesc.base() == key ) return true;
    std::string base = mDesc.base();
    std::string param = mDesc.param();
    if( key.substr( 0, base.length() ) != base ){
      return false;
    }
    if( param.length() == 0 ) return false;
    if( param == "d" ){
      size_t pos = key.find_first_not_of( "0123456789", base.length() );
      if( pos == std::string::npos ){
	return true;
      }
    } else if( param == "x" ){
      size_t pos = key.find_first_not_of( "0123456789abcdefABCDEF",
					  base.length() );
      if( pos == std::string::npos ){
	return true;
      }
    }
    return false;
  }
};

class ConfigClass
{
  Description mDesc;

public:
  std::vector< ConfigValue > mValues;
  ConfigClass()
  {}
  ConfigClass( const char * str, size_t len )
    : mDesc( str )
  {}
  const std::string & className() const
  {
    return mDesc.base();
  }
};
class ConfigSetup
{
  bool mError;
  std::string mErrorMessage;
public:
  enum Modes { nullMode, mSections, mStartArray, mValues };
  Modes mMode;
  ConfigClass mCurrentConfig;
  std::map< std::string, ConfigValue> mAllConfigs;
  ConfigSetup()
    : mError( false )
    , mMode( nullMode )
  {}
  std::map<std::string, ConfigClass> mConfigs;
  void setError( const char * message = nullptr )
  {
    mError = true;
    if( message ) {
      mErrorMessage = message;
    }
  }
  class Reader : public json_lite::ReaderHandlerAllFail
  {
    ConfigSetup & mParent;
  public:
    Reader(  ConfigSetup & parent )
      : mParent( parent )
    {}
    void setError( const char * message = nullptr ) override
    {
      mParent.setError( message );
    }
    bool String( const char * str, size_t len )
    {
      return mParent.onString( str, len );
    }
    bool Key( const char * str, size_t len )
    {
      return mParent.onKey( str, len );
    }
    bool StartObject()
    {
      return mParent.onStartObject();
    }
    bool EndObject()
    {
      return mParent.onEndObject();
    }
    bool StartArray()
    {
      return mParent.onStartArray();
    }
    bool EndArray()
    {
      return mParent.onEndArray();
    }
  };
  bool onKey( const char * str, size_t len )
  {
    if( mMode == mSections ){
      mMode = mStartArray;
      ConfigClass nextClass( str, len );
      mCurrentConfig = nextClass;
      return true;
    }
    setError( "Unexpeced key" );
    return false;
  }
  bool onStartArray()
  {
    if( mMode == mStartArray ){
      mMode = mValues;
      return true;
    }
    return false;
  }
  bool onEndArray()
  {
    if( mMode == mValues ){
      mMode = mSections;
      mConfigs[ mCurrentConfig.className() ] =  mCurrentConfig;
      return true;
    }
    setError( "End of Array unexpected" );
    return false;
  }
  bool onEndObject()
  {
    if( mMode == mSections ){
      mMode = nullMode;
      return true;
    }
    return false;
  }
  bool onStartObject( )
  {
    if( mMode == nullMode ){
      mMode = mSections;
      return true;
    }
    setError( "Unexpected StartObject" );
    return false;
  }
  bool onString( const char * str, size_t len )
  {
    if( mMode == mValues ){
      ConfigValue newValue( str );
      newValue.mClass = mCurrentConfig.className();
      mAllConfigs[ newValue.mDesc.base() ] = newValue;
      mCurrentConfig.mValues.push_back( newValue );
      return true;
    }
    return false;
  }
  
  bool findValue( const std::string & key, ConfigValue & val ) const
  {
    std::string _key = key;
    while( _key.length() > 0 ){
      auto it = mAllConfigs.find( _key );
      if( it != mAllConfigs.end() ){
	if( _key != key ){
	  if (! it->second.isValid( key ) ){
	    return false;
	  }
	}
	val = it->second;
	return true;
      }
      _key.resize( _key.length() -1 );
    }
    return false;
  }
};
ConfigSetup buildConfig( const char * config )
{
  ConfigSetup newConfig;
  json_lite::StringInputStream strStream( config );
  ConfigSetup::Reader handler(newConfig );
  json_lite::Reader rdr( handler, strStream, true );
  rdr.read( true );
  return newConfig;

}
struct option config_edit_options[] =
  {
   { "platform", required_argument, nullptr, 'p'},
   { "edid",     required_argument, nullptr, 'e'},
   { "all",      no_argument,       nullptr, 0 }, 
   { "add",      required_argument, nullptr, 'a'},
   { "remove",   required_argument, nullptr, 'r'},
   { "comment",  required_argument, nullptr, 'c'},
   { "file",     required_argument, nullptr, 'f'},
   { "print",    no_argument,       nullptr, 0 },
   { "gpio",     required_argument, nullptr, 'g' },
   { "config",   required_argument, nullptr, 0 },
   { "hdmi",     required_argument, nullptr, 0 },
   { nullptr,    0,                 nullptr, 0 },
  };


bool Section::sectionChange( const std::string & line,
			     const ConfigSetup & config )
{
  std::string key, value;
  if( Filter::parseFilter( line, key,value ) ){
    ConfigValue val;
    if( config.findValue( key, val ) ){
      if( val.mClass == "super" ) { // Special case.
	                            // Remove all other filters
	mSelection.clear();
      } else {
	std::vector<Filter> newSelections;
	for( auto it = mSelection.begin(); it != mSelection.end(); it++ ){
	  if( it->mClass != val.mClass ){ // keep all filters not current
	    newSelections.push_back( *it );
	  }
	}
	std::swap( mSelection, newSelections );
      }
      Filter flt( val.mClass, key, value, line );
      mEntryFilter = flt;
      mSelection.push_back( flt );
      //for( auto it = mSelection.begin(); it != mSelection.end(); it++ ){
      //printf( "%s(%s) - %s\n", it->mKey.c_str(),
      //	it->mValue.c_str(),
      //	it->mClass.c_str() );
      //}
      
      return true;
    }
    
  } 
  return false;
}

using namespace json_lite;


WholeFile readWholeFile( std::istream & input, ConfigSetup & config)
{
  std::string line;
  WholeFile file;
  Section currentSection;
  while( std::getline( input, line ) ){
    if( line.length() && line[0] == '[' &&
	line.find( ']' ) != std::string::npos ){
      file.mSections.push_back( currentSection );
      currentSection.mLines.clear(); // start a new section
      currentSection.sectionChange( line, config );
    } else {
      currentSection.mLines.push_back( line );
    }
  }
  file.mSections.push_back( currentSection );
  return file;
}
void doDisplayConfig( const WholeFile & theFile )
{
  for( auto sections = theFile.mSections.begin(); sections != theFile.mSections.end() ; sections++ ){
    if( sections->mEntryFilter.mEmpty == false ){
      std::cout << sections->mEntryFilter.mLine << std::endl;
    }

    if( sections->mSelection.size() ){
      std::cout << "##################################################" << std::endl;
      std::cout << "# Active filters" << std::endl;
      for( auto flt = sections->mSelection.begin(); flt != sections->mSelection.end(); flt++ ){
	std::cout << "# " << flt->mClass << " " << flt->mKey << " " << flt->mValue << std::endl;
      }
      std::cout << "##################################################" << std::endl;
    }
    for( auto line = sections->mLines.begin(); line != sections->mLines.end(); line++ ){
      std::cout << *line << std::endl;
    }
  }

  
}
void displayConfig( ConfigSetup & setup, const std::string & fileName )
{
  std::ifstream file;
  file.open( fileName );
  WholeFile theFile = readWholeFile( file, setup );
  doDisplayConfig( theFile );
}

struct Actions
{
  std::vector< Filter > requiredFilters;
  std::vector< std::string> addCommands;
  std::vector< std::string> removeCommands;
  std::vector< std::string> commentCommands;
};
void editConfig( ConfigSetup & cfg, const std::string & fileName, const Actions & actions )
{
  WholeFile theFile;
  {
    std::ifstream file;
    file.open( fileName );
    theFile = readWholeFile( file, cfg );
  }
  std::vector< Section>::iterator lastMatch = theFile.mSections.end();
  for( auto section = theFile.mSections.begin(); section != theFile.mSections.end() ; section++ ){
    if( section->matches( actions.requiredFilters ) ){
      std::cerr << "# Section starting with " << section->mEntryFilter.mLine << "Matches" << std::endl;
      lastMatch = section;
    }
  }
  if( lastMatch != theFile.mSections.end() ){
    // comments
    for( auto cmd = actions.commentCommands.begin(); cmd!= actions.commentCommands.end(); cmd ++ ){
      std::string _c = *cmd;
      for( auto line = lastMatch->mLines.begin(); line != lastMatch->mLines.end(); lastMatch++ ){
	std::string _l = *line;
	if( _l == _c ){
	  std::string replacement = "#";
	  replacement += _l;
	  *line = replacement;
	}
      }
    }
    // deletes
    for( auto cmd = actions.removeCommands.begin(); cmd!= actions.removeCommands.end(); cmd ++ ){
      std::string _c = *cmd;
      for( auto line = lastMatch->mLines.begin(); line != lastMatch->mLines.end(); lastMatch++ ){
	std::string _l = *line;
	if( _l == _c ){
	  lastMatch->mLines.erase( line );
	  break;
	}
      }
    }
    // inserts
    for( auto cmd = actions.addCommands.begin(); cmd != actions.addCommands.end(); cmd++ ){
      lastMatch->mLines.push_back( *cmd );
    }
  } else {
    theFile.resetToAll();
    Section currentSection;
    for( auto flt = actions.requiredFilters.begin(); flt != actions.requiredFilters.end(); flt++ ){
      currentSection.sectionChange( flt->mLine, cfg);
      theFile.mSections.push_back( currentSection );
    }
    for( auto cmd = actions.addCommands.begin(); cmd != actions.addCommands.end(); cmd++ ){
      theFile.addLine( *cmd );
    }
    
  }
  doDisplayConfig( theFile );
}
int main( int argc, char * argv[] )
{
  std::string file = "/boot/config.txt";
  bool bInvalid = false;
  bool bPrintMode = false;
  Actions actions;
  while ( bInvalid == false) {
    int opt_idx = 0;
    int c = getopt_long( argc, argv, "p:e:a:r:c:f:g:", config_edit_options, &opt_idx);
    if( c == -1 ) {
      break;
    }
    switch( c ){
    case '?':
      bInvalid = true;
      break;
    case 'f':
      file = optarg;
      break;
    case 0:
    default:
      {
	std::string option = config_edit_options[ opt_idx].name;
	if( option == "print" ){
	  bPrintMode = true;
	} else if( option == "platform" ||
		   option == "edid" ||
		   option == "gpio" ||
		   option == "cpuserial" ){
	  Filter flt( optarg, option.c_str() );
	  actions.requiredFilters.push_back( flt );
	} else if( option == "add" ) {
	  actions.addCommands.push_back( optarg );
	} else if ( option == "remove" ) {
	  actions.removeCommands.push_back( optarg );
	} else if ( option == "comment" ) {
	  actions.commentCommands.push_back( optarg );
	}
	printf( "Option %s", config_edit_options[ opt_idx].name );
	if( optarg ){
	  printf( " %s", optarg );
	}
	printf( "\n" );
	break;
      }
    }
  }
  if( bInvalid ){
    exit( 1 );
  }
  ConfigSetup cfg = buildConfig( config );
  /////////////////////////////////////////////////////////
  // ensure all the Filters added to actions are valid.
  {
    bool allOk = true;
    for( auto it = actions.requiredFilters.begin(); allOk == true && it != actions.requiredFilters.end(); it++ ){
      std::string key = it->mKey;
      if( it->mValue.length() > 0 ){
	key+= "=";
	key+= it->mValue;
      }
      ConfigValue tmp;
      if( !cfg.findValue( it->mKey,tmp ) ){
	allOk = false;
	std::cerr << "Invalid filter '" << key << "'" << std::endl;
      }
    }
    if( allOk == false ){
      return  1;
    }
  }
  //
  ////////////////////////////////////////////////////////
  if( bPrintMode ){
    displayConfig( cfg, file );
  } else {
    editConfig( cfg, file, actions );
  }
  //  test2( config );
  Description desc( "gpio%d" );
  Description desc2( "0x%x" );
  Description desc3( "pi0" );
  
}
