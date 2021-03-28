#include <getopt.h>
#include <string>
#include <map>
#include <vector>
#include <fstream>
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
private:
  std::string mClass;
  std::string mKey;
  std::string mValue;
  std::string mLine;
  bool mEmpty;
public:
  Filter()
    : mEmpty( true )
  {}
  Filter( const char * fltClass, const char * key
	  , const char * value, const char * line )
    : mClass( fltClass )
    , mKey( key )
    , mValue( value )
    , mLine( line )
    , mEmpty( false )
  {
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
  void sectionChange( const std::string & line, const ConfigSetup & config );
};

class WholeFile
{
public:
  std::vector<Section> mSections;
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
  "display" :   [ "edid" ],
  "cpuserial" : [ "0x%x" ], 
  "HDMI" :      [ "HDMI:0", "HDMI:1" ],
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
  bool isValid( const std::string & key )
  {
    if( mBase == mKey ) return true;
    if( key.substr( 0,mBase.length() ) != mBase ){
      return false;
    }
    if( mParam.length() == 0 ) return false;
    if( mParam == "d" ){
      size_t pos = key.find_first_not_of( "0123456789", mBase.length() );
      if( pos == string::npos ){
	return true;
      }
    } else if( mParam == "x" ){
      size_t pos = key.find_first_not_of( "0123456789abcdefABCDEF",
					  mBase.length() );
      if( pos == string::npos ){
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
  
  bool findValue( const std::string & key, ConfigValue & val )
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
   { nullptr,    0,                 nullptr, 0 },
  };


bool Section::sectionChange( const std::string & line,
			     const ConfigSetup & config )
{
  std::string key, value;
  if( Filter::parseFilter( line, key,value ) ){
    ConfigValue val;
    if( config.findValue( key, val ) ){
      if( val.mClass == "super" ) { // boo special case.  Remove all other filters
      } else {
	
      }
    }
  } 
  return false;
}

using namespace json_lite;
void displayConfig( ConfigSetup & setup, const std::string & fileName )
{
  std::ifstream file;
  file.open( fileName );
  std::string line;
  while( std::getline(file, line ) ){
    if( line.length() && line[0] == '[' && line.find( ']' ) != std::string::npos ){
      printf( "filter %s\n", line.c_str() );
    } else {
      printf( "%s\n", line.c_str() );
    }
  }
}
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
      printf( "filter %s\n", line.c_str() );
    } else {
      currentSection.mLines.push_back( line );
      printf( "%s\n", line.c_str() );
    }
  }
  return file;
}

int main( int argc, char * argv[] )
{
  std::string file = "/boot/config.txt";
  bool bInvalid = false;
  bool bPrintMode = false;
  while ( bInvalid == false) {
    int opt_idx = 0;
    int c = getopt_long( argc, argv, "p:e:a:r:c:f:", config_edit_options, &opt_idx);
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
  if( bPrintMode ){
    displayConfig( cfg, file );
  }
  //  test2( config );
  Description desc( "gpio%d" );
  Description desc2( "0x%x" );
  Description desc3( "pi0" );
  
}
