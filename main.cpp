#include <getopt.h>
#include <string>
#include <map>
#include <vector>
#include "json_lite.h"

class Filter
{
private:
  std::string mClass;
  std::string mValue;
  std::string mLine;
public:
  Filter( const char * fltClass, const char * value, const char * line )
    : mClass( fltClass )
    , mValue( value )
    , mLine( line )
  {
  }
};
class Section
{
  std::vector< Filter > mSelection;  // which filters are active...
  Filter mEntryFilter;
  std::vector< std::string > mLines;
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
  "HDMI" :      [ "hdmi:0", "hdmi:1" ],
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
    }
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
   { nullptr,    0,                 nullptr, 0 },
  };

using namespace json_lite;

int main( int argc, const char * argv[] )
{
  test( " ]", false );
  test( " 12.31234" );
  test( " \"hello\"" );
  buildConfig( config );
  //  test2( config );
  Description desc( "gpio%d" );
  Description desc2( "0x%x" );
  Description desc3( "pi0" );
  
}
