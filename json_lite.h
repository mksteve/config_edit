//////////////////////////////////////////////////////////
//  json lite parser - based on ideas of SAX XML parser
#pragma once
#if ! defined( H_JSON_LITE_H)
#define H_JSON_LITE_H
#include <cctype>
#include <string.h>
#include <string>
#include <algorithm>
#include <assert.h>

namespace json_lite {
  // parser based on grammar
  // https://www.json.org/json-en.html
  class ReaderHandler
  {
  public:
    virtual bool Null() = 0;
    virtual bool Bool( bool b ) = 0;
    virtual bool Int( int i ) = 0;
    virtual bool Uint( unsigned u ) = 0;
    virtual bool Int64( int64_t i ) = 0;
    virtual bool Uint64( uint64_t u ) = 0;
    virtual bool Double( double d ) = 0;
    virtual bool String( const char * str, size_t size) = 0;
    virtual bool RawNumber( const char * str, size_t length ) = 0;
    virtual bool StartObject() = 0;
    virtual bool Key( const char * str, size_t length ) = 0;
    virtual bool EndObject() = 0;
    virtual bool StartArray() = 0;
    virtual bool EndArray() = 0;
  };

  class InputStream
  {
  public:
    virtual bool getch( char & ch) = 0;
    virtual bool ungetch( const char  ch ) = 0;
  };

  class StringInputStream : public InputStream
  {
    const char * mPos;
    char mLastChar;
    bool mLastCharValid;
  public:
    StringInputStream( const char * stream )
      : mPos( stream )
      , mLastChar(0)
      , mLastCharValid( false )
    {}
    bool ungetch( const char ch )
    {
      if( mLastCharValid == true )
	return false;
      mLastChar = ch;
      mLastCharValid = true;
      return true;
    }
    bool getch( char & ch) override
    {
      if( mLastCharValid ){
	mLastCharValid = false;
	ch = mLastChar;
	mLastChar = 0;
	return true;
      }
      if( mPos && *mPos ){
	ch = *mPos;
	mPos++;
	return true;
      }
      return false;
    }
  };

  class Token
  {
  public:
    enum TokenType { tkNull, tkTrue, tkFalse, tkString, tkNumber, tkStartObject, tkEndObject, tkStartArray, tkEndArray, tkComma, tkColon };
  private:
    std::string mValue;
    TokenType   mToken;
  public:
    bool symbol( const std::string & str ) 
    {
      if( str == "true" ){
	mToken = tkTrue;
	return true;
      }
      if( str == "false" ){
	mToken = tkFalse;
	return true;
      }
      if( str == "null" ){
	mToken = tkNull;
	return true;
      }
      return false;
    }
    bool isValue()
    {
      switch( mToken ){
      default:
	return false;
      case  tkNull: case tkTrue: case tkFalse:
      case tkString: case tkNumber:
      case tkStartObject: case tkStartArray:
	return true;
      }
    }
    bool sym( const char sym )
    {
      switch( sym ){
      default:
	return false;
      case '{':
	mToken = tkStartObject;
	return true;
      case '}':
	mToken = tkEndObject;
	return true;
      case '[':
	mToken = tkStartArray;
	return true;
      case ']':
	mToken = tkEndArray;
	return true;
      case ':':
	mToken = tkColon;
	return true;
      case ',':
	mToken = tkComma;
	return true;
      }
      return false;
    }
    Token & operator=( const std::string & rhs )
    {
      mValue = rhs;
      mToken = tkString;
      return *this;
    }
    const char * str() const
    {
      return mValue.c_str();
    }
    size_t len() const
    {
      return mValue.length();
    }
    TokenType type() const
    {
      return mToken;
    }
  };

  class Reader
  {
    size_t mObjectDepth;
    ReaderHandler & mEvents;
    InputStream   & mInput;
    bool            mStrict; /* Require "round keys? */
    enum ReaderMode {  inObject,
		       inObjectGotKey,
		       inObjectGotColon,
		       inObjectGotValue,
		       inObjectCommaOrEnd,
		       inObjectGotComma,

		       inArray,
		       inArrayGotValue,
		       inArrayGotComma,
		     
		       topValue,

		       done};
    bool readToken_prv( Token & token, bool isStrict )
    {
      if( readWhitespace() == false ) return false;
      std::string tmp;
      char ch;
      if( mInput.getch( ch ) == false ) return false;
      if( isSingleCharToken( ch ) ){
	token.sym( ch );
	return true;
      }
      if(  ch == '-' || isdigit( ch ) ){
	mInput.ungetch( ch );
	bool result = readNumber( tmp );
	if( result ){
	  token = tmp;
	}
	return result;
      }
      mInput.ungetch( ch );
      bool result = readString( tmp, smLax );
      if( result == false ) return result;
      if( ch != '"' ){
	if( tmp == "true" || tmp == "false" || tmp == "null" ){
	  token.symbol( tmp );
	  return true;
	}
      }
      if( ch != '"' && mStrict ){
	return false;
      }
      token = tmp;
      return true;
    }

  public:
    class Grammar
    {
      std::vector< Token::TokenType> mNext;
    public:
      Grammar()
      {}
      template <size_t sz>
      Grammar( const Token::TokenType (& nextSyms)[sz] )
      : mNext( nextSyms, nextSyms + sz )
      {
      }
      bool isValid( const Token::TokenType & next ) const
      {
	if( std::find(mNext.begin(), mNext.end(), next) != mNext.end() ){
	  return true;
	}
	return false;
      }
    };
    enum StrictMode { smDefault, smStrict, smLax };
    Reader( ReaderHandler & reader, InputStream & stream, bool strict = true )
      : mObjectDepth(0)
      , mEvents( reader )
      , mInput( stream )
    {
    }
    bool readWhitespace()
    {
      char ch;
      while( mInput.getch( ch ) == true ){
	if( isspace( ch ) ){
	} else {
	  mInput.ungetch( ch );
	  return true;
	}
      }
      return false;
    }
    bool gatherDigits( std::string & gatherer )
    {
      char ch;
      while( mInput.getch( ch ) == true ){
	if( isdigit( ch ) ){
	  gatherer+= ch;
	} else {
	  mInput.ungetch( ch );
	  return true;
	}
      }
      return true;
    }
    bool isSingleCharToken( char ch )
    {
      static const char * single_char_tokens = "[]{},:";
      if( strchr( single_char_tokens, ch ) != nullptr ){
	return true;
      }
      return false;
    }
    bool readNumber( std::string & token )
    {
      std::string gather;
      char ch;
      char firstCh;
      if(mInput.getch( firstCh ) == false ){
	return false;
      }
      gather += firstCh;
      if( firstCh != '0' ){ // any digits up to possible .
	gatherDigits( gather );
      }
      if( mInput.getch( ch ) == false ){
	token = gather;
	mInput.ungetch( ch );
	return true;
      }
      if( ch == '.' ){
	gather += ch;
	gatherDigits( gather );
      }
      if( mInput.getch( ch ) == false ) {
	token = gather;
	return true;
      }
      if( ch == 'E' || ch == 'e' ){
	gather += ch;
      }
      if( mInput.getch( ch ) == false ) { // not allowed 1.2e
	return false;
      }
      if( ch == '-' || ch == '+' ){
	gather += ch;
      }
      std::string exponent;

      if( gatherDigits( exponent ) == false ){
	return false;
      }
      if( exponent.length() == 0 ){
	return false;
      }
      gather += exponent;
      token = gather;
      return true;
    }
    bool readString( std::string & token, enum StrictMode strictMode = smDefault )
    {
      bool isStrict = true;
      char entryChar;
      if( mInput.getch( entryChar ) == false ){
	return false;
      }
      if( entryChar != '"' &&
	  (
	   ((strictMode == smStrict || strictMode == smDefault) && mStrict ) ||
	   ((strictMode == smStrict )) )){
	return false;
      }
      std::string gather;
      if( entryChar != '"' ){
	isStrict = false;
	gather = entryChar;
      }
      char ch;
      while( mInput.getch( ch ) == true ) {
	if( ch == '"' ){
	  if( isStrict ) {
	    token = gather;
	    return  true;
	  } else {
	    return false;
	  }
	} else if ( ch == '\\' ){
	  if( mInput.getch( ch ) == false ){
	    return false;
	  }
	  if( isxdigit( ch ) ){
	    std::string hexValue;
	    hexValue = ch;
	    for( size_t i = 0; i < 3 ; i++ ){
	      if( mInput.getch( ch ) == false ){
		return false;
	      }
	      if( isxdigit( ch ) == false ){
		return false;
	      }
	      hexValue += ch;
	    }
	    int chValue = std::stoi( hexValue, nullptr, 16 );
	    gather += chValue;
	  } else {
	    switch( ch ){
	    default:
	      return false;
	    case '"':
	      gather += '"';
	      break;
	    case '\\':
	      gather += '\\';
	      break;
	    case '/':
	      gather += '/';
	      break;
	    case 'b':
	      gather += '\f';
	      break;
	    case 'n':
	      gather += '\n';
	      break;
	    case 'r':
	      gather += '\r';
	      break;
	    case 't':
	      gather += '\t';
	      break;
	    }
	  }
	} else {
	  // any codepoint except " \ or control characters
	  if( iscntrl( ch ) ){
	    return false;
	  }
	  if( isStrict == false ){
	    if( isspace( ch) || isSingleCharToken( ch ) ){
	      mInput.ungetch( ch );
	      token = gather;
	      return true;
	    }
	  }
	  gather += ch;
	}
      }
      if( isStrict ){
	return false;
      } else {
	token = gather;
	return true;
      }
    }
    bool readToken( Token & token, bool isStrict, const Grammar * next )
    {
      Token temp;
      if( readToken_prv( temp, isStrict ) ){
	if( next == nullptr || next->isValid( temp.type() ) ){
	  token = temp;
	  return true;
	}
      }
      return false;
    }
    bool sendNumber( const Token & currentToken )
    {
      mEvents.RawNumber( currentToken.str(), currentToken.len() );
      double dbl;
      if( strcspn( currentToken.str(), ".eE") != currentToken.len() ){
	if( sscanf(currentToken.str(), "%f", &dbl ) == 1 ){
	  mEvents.Double( dbl );
	  return true;
	}
      } else {
	int64_t  llInt = 0;
	llInt = std::strtoll( currentToken.str(), nullptr, 10 );
	mEvents.Int( llInt );
	return true;
      }
      return false;
    }
    bool processValue( const Token & tok, bool & complex )
    {
      bool bDone = false;
      complex = false;
      switch( tok.type() )
	{
	case Token::tkTrue:
	case Token::tkFalse:
	  mEvents.Bool( tok.type() == Token::tkTrue );
	  bDone = true;
	  break;
	case Token::tkNull:
	  mEvents.Null();
	  bDone = true;
	  break;
	case Token::tkNumber:
	  bDone = sendNumber( tok );
	  break;
	case Token::tkString:
	  mEvents.String( tok.str(), tok.len() );
	  bDone = true;
	  break;
	case Token::tkStartObject:
	  mEvents.StartObject();
	  complex = true;
	  bDone = true;
	  break;
	case Token::tkStartArray:
	  mEvents.StartArray();
	  complex = true;
	  bDone = true;
	  break;
	}
      return bDone;
    }
    bool read( bool isStrict )
    {
      enum ComplexType { ctObject, ctArray };
      struct ReaderStack {
	ReaderMode mMode;
	const Grammar * currentGrammar;
	ComplexType mType;
      };
      std::vector< ReaderStack> modeStack;
      ReaderMode currentMode = topValue;

      static const Grammar valueGrammar ({Token::tkNull, Token::tkTrue, Token::tkFalse,
					  Token::tkString, Token::tkNumber,
					  Token::tkStartObject, Token::tkStartArray });

      static const Grammar objectStartGrammar( {Token::tkEndObject, Token::tkString } );
      static const Grammar objectColonGrammar( {Token::tkColon } );
      static const Grammar objectAfterValueGrammar( {Token::tkComma, Token::tkEndObject} );
      static const Grammar objectKeyGrammar( {Token::tkString } );

      static const Grammar arrayStartGrammar( {Token::tkNull, Token::tkTrue, Token::tkFalse,
					       Token::tkString, Token::tkNumber,
					       Token::tkStartObject, Token::tkStartArray ,
					       Token::tkEndArray} );
      static const Grammar arrayAfterValueGrammar( {Token::tkComma, Token::tkEndArray} );
      static const Grammar nothingGrammar;
      const Grammar * currentGrammar = &valueGrammar;

      Token currentToken;
      bool startComplex;
      while( readToken( currentToken, isStrict, currentGrammar ) == true ){
	startComplex = false;

	if( currentToken.isValue() ){
	  if( currentMode == inObject || currentMode == inObjectGotComma ){
	    mEvents.Key( currentToken.str(), currentToken.len() );
	  } else {
	    processValue( currentToken, startComplex );
	  }
	}
	if( currentToken.type() == Token::tkStartObject ){
	  ReaderStack newElem { currentMode, currentGrammar, ctObject };
	  modeStack.push_back( newElem );
	  currentMode = inObject;
	  currentGrammar = &objectStartGrammar;
	} else if( currentToken.type() == Token::tkStartArray ){
	  ReaderStack newElem { currentMode, currentGrammar, ctArray };
	  modeStack.push_back( newElem );
	  currentMode = inArray;
	  currentGrammar = &arrayStartGrammar;
	} else {
	  if ( currentToken.type() == Token::tkEndObject ){
	    if( modeStack.size() == 0 ) return false;
	    ReaderStack st = *modeStack.rbegin();
	    if( st.mType != ctObject ) return false;
	    modeStack.pop_back();
	    mEvents.EndObject();
	    currentMode = st.mMode;
	    currentGrammar = st.currentGrammar;
	  } else if ( currentToken.type() == Token::tkEndArray ){
	    if( modeStack.size() == 0 ) return false;
	    ReaderStack st = *modeStack.rbegin();
	    modeStack.pop_back();
	    if( st.mType != ctArray ) return false;
	    currentMode = st.mMode;
	    currentGrammar = st.currentGrammar;
	    mEvents.EndArray();
	  }
	  switch( currentMode ){
	  case topValue:
	    currentMode = done;
	    currentGrammar = &nothingGrammar;
	    break;
	  case inObject:
	  case inObjectGotComma:
	    if( currentToken.type() == Token::tkString ){
	      currentMode = inObjectGotKey;
	      currentGrammar = &objectColonGrammar;
	    }
	    break;
	  case inObjectGotKey:
	    assert( currentToken.type() == Token::tkColon );
	    currentMode = inObjectGotColon;
	    currentGrammar = &valueGrammar;
	    break;
	  case inObjectGotValue:
	    currentMode = inObjectCommaOrEnd;
	    currentGrammar = &objectAfterValueGrammar;
	    break;
	  case inObjectGotColon:
	    currentMode = inObjectCommaOrEnd;
	    currentGrammar = &objectAfterValueGrammar;
	    break;
	  case inObjectCommaOrEnd:
	    if( currentToken.type() ==Token::tkComma ){
	      currentMode = inObjectGotComma;
	      currentGrammar = &objectKeyGrammar;
	    }
	    break;
	  case inArray:
	  case inArrayGotComma:
	    if( currentToken.isValue() ){
	      currentMode = inArrayGotValue;
	      currentGrammar = &arrayAfterValueGrammar;
	    }
	    break;
	  case inArrayGotValue:
	    if( currentToken.type() != Token::tkEndArray ) {
	      currentMode = inArrayGotComma;
	      currentGrammar = &valueGrammar;
	    }
	    break;
	  }
	}
      }
      if( mObjectDepth != 0 || modeStack.size() != 0 ){
	return false;
      }
      return true;
    }
  };
  class ReaderHandlerAllFail : public ReaderHandler
  {
  public:
    bool mError;
    std::string mErrorMessage;
    ReaderHandlerAllFail()
      : mError( false )
    {}
    virtual void setError( const char * message = nullptr )
    {
      mError = true;
      if( message ) {
	mErrorMessage = message;
      }
    }
    bool Null()
    {
      setError( "Unexpected null" );
      return false;
    }
    bool Bool( bool b )
    {
      setError( "Unexpected bool" );
      return false;
    }
    bool Int( int number )
    {
      setError( "Unexpected integer" );
      return false;
    }
    bool Uint( unsigned number )
    {
      setError( "Unexpected integer" );
      return false;
    }
    bool Int64( int64_t number )
    {
      setError( "Unexpected integer" );
      return false;
    }
    bool Uint64(  uint64_t n )
    {
      setError( "Unexpected integer" );
      return false;
    }
    bool Double( double d )
    {
      setError( "Unexpected double" );
      return false;
    }
    bool RawNumber( const char * str, size_t len )
    {
      return true;
    }

    bool String( const char * str, size_t len )
    {
      setError( "Unexpected String" );
      return false;
    }
    bool Key( const char * str, size_t len )
    {
      setError( "Unexpected key" );
      return false;
    }
    bool StartObject()
    {
      setError( "Unexpected StartObject" );
      return false;
    }
    bool EndObject()
    {
      setError( "Unexpected EndObject" );
      return false;
    }
    bool StartArray()
    {
      setError( "Unexpected StartArray" );
      return false;

    }
    bool EndArray()
    {
      setError( "Unexpected EndArray" );
      return false;

    }

  };
};
#endif

