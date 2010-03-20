/*
 * Software License :
 *
 * Copyright (c) 2007-2009, The Open Effects Association Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * Neither the name The Open Effects Association Ltd, nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// ofx
#include "ofxCore.h"
#include "ofxImageEffect.h" ///@todo tuttle: remove this

// ofx host
#include "OfxhBinary.hpp"
#include "OfxhProperty.hpp"
#include "OfxhParam.hpp"
#include "OfxhImageEffectNode.hpp" ///@todo tuttle: remove this

#include <cassert>
#include <cfloat>
#include <climits>

#ifdef _MSC_VER
	#undef max
	#undef min
	#include <limits>
#endif

#include <cstdarg>

namespace tuttle {
namespace host {
namespace ofx {
namespace attribute {

OfxhParamAccessor::OfxhParamAccessor()
{}

OfxhParamAccessor::~OfxhParamAccessor() {}

const std::string& OfxhParamAccessor::getParamType() const
{
	return getProperties().getStringProperty( kOfxParamPropType );
}

const std::string& OfxhParamAccessor::getParentName() const
{
	return getProperties().getStringProperty( kOfxParamPropParent );
}

const std::string& OfxhParamAccessor::getScriptName() const
{
	return getProperties().getStringProperty( kOfxParamPropScriptName );
}

const std::string& OfxhParamAccessor::getHint() const
{
	return getProperties().getStringProperty( kOfxParamPropHint, 0 );
}

const std::string& OfxhParamAccessor::getDoubleType() const
{
	return getProperties().getStringProperty( kOfxParamPropDoubleType, 0 );
}

const std::string& OfxhParamAccessor::getCacheInvalidation() const
{
	return getProperties().getStringProperty( kOfxParamPropCacheInvalidation, 0 );
}

bool OfxhParamAccessor::getEnabled() const
{
	return getProperties().getIntProperty( kOfxParamPropEnabled, 0 ) != 0;
}

bool OfxhParamAccessor::getSecret() const
{
	return getProperties().getIntProperty( kOfxParamPropSecret, 0 ) != 0;
}

bool OfxhParamAccessor::getEvaluateOnChange() const
{
	return getProperties().getIntProperty( kOfxParamPropEvaluateOnChange, 0 ) != 0;
}

bool OfxhParamAccessor::getCanUndo() const
{
	if( getProperties().hasProperty( kOfxParamPropCanUndo ) )
	{
		return getProperties().getIntProperty( kOfxParamPropCanUndo ) != 0;
	}
	return false;
}

bool OfxhParamAccessor::getCanAnimate() const
{
	if( getProperties().hasProperty( kOfxParamPropAnimates ) )
	{
		return getProperties().getIntProperty( kOfxParamPropAnimates ) != 0;
	}
	return false;
}

//
// Descriptor
//

struct TypeMap
{

	const char* paramType;
	property::TypeEnum propType;
	int propDimension;
};

bool isDoubleParam( const std::string& paramType )
{
	return paramType == kOfxParamTypeDouble ||
	       paramType == kOfxParamTypeDouble2D ||
	       paramType == kOfxParamTypeDouble3D;
}

bool isColourParam( const std::string& paramType )
{
	return
	    paramType == kOfxParamTypeRGBA ||
	    paramType == kOfxParamTypeRGB;
}

bool isIntParam( const std::string& paramType )
{
	return paramType == kOfxParamTypeInteger ||
	       paramType == kOfxParamTypeInteger2D ||
	       paramType == kOfxParamTypeInteger3D;
}

static TypeMap typeMap[] = {
	{ kOfxParamTypeInteger, property::eInt, 1 },
	{ kOfxParamTypeDouble, property::eDouble, 1 },
	{ kOfxParamTypeBoolean, property::eInt, 1 },
	{ kOfxParamTypeChoice, property::eInt, 1 },
	{ kOfxParamTypeRGBA, property::eDouble, 4 },
	{ kOfxParamTypeRGB, property::eDouble, 3 },
	{ kOfxParamTypeDouble2D, property::eDouble, 2 },
	{ kOfxParamTypeInteger2D, property::eInt, 2 },
	{ kOfxParamTypeDouble3D, property::eDouble, 3 },
	{ kOfxParamTypeInteger3D, property::eInt, 3 },
	{ kOfxParamTypeString, property::eString, 1 },
	{ kOfxParamTypeCustom, property::eString, 1 },
	{ kOfxParamTypeGroup, property::eNone },
	{ kOfxParamTypePage, property::eNone },
	{ kOfxParamTypePushButton, property::eNone },
	{ 0 }
};

/// is this a standard type

bool isStandardType( const std::string& type )
{
	TypeMap* tm = typeMap;

	while( tm->paramType )
	{
		if( tm->paramType == type )
			return true;
		++tm;
	}
	return false;
}

bool findType( const std::string paramType, property::TypeEnum& propType, int& propDim )
{
	TypeMap* tm = typeMap;

	while( tm->paramType )
	{
		if( tm->paramType == paramType )
		{
			propType = tm->propType;
			propDim  = tm->propDimension;
			return true;
		}
		++tm;
	}
	return false;
}

/**
 * @brief make a parameter, with the given type and name
 */
OfxhParamDescriptor::OfxhParamDescriptor( const std::string& type, const std::string& name )
	: attribute::OfxhAttributeDescriptor( property::OfxhSet() )
{
	const char* ctype = type.c_str();
	const char* cname = name.c_str();

	static const property::OfxhPropSpec paramDescriptorProps[] = {
		{ kOfxPropType, property::eString, 1, true, kOfxTypeParameter },
		{ kOfxParamPropSecret, property::eInt, 1, false, "0" },
		{ kOfxParamPropHint, property::eString, 1, false, "" },
		{ kOfxParamPropParent, property::eString, 1, false, "" },
		{ kOfxParamPropEnabled, property::eInt, 1, false, "1" },
		{ kOfxParamPropDataPtr, property::ePointer, 1, false, 0 },
		{ 0 }
	};

	const property::OfxhPropSpec dynamicParamDescriptorProps[] = {
		{ kOfxParamPropType, property::eString, 1, true, ctype },
		{ kOfxParamPropScriptName, property::eString, 1, false, cname }, ///< @todo TUTTLE_TODO : common property for all Attributes
		{ 0 }
	};

	getEditableProperties().addProperties( paramDescriptorProps );
	getEditableProperties().addProperties( dynamicParamDescriptorProps );

	setAllNames( name );

	getEditableProperties().setStringProperty( kOfxParamPropType, type );
	assert( ctype );
}

/**
 * make a parameter, with the given type and name
 */
void OfxhParamDescriptor::initStandardParamProps( const std::string& type )
{
	property::TypeEnum propType = property::eString;
	int propDim                 = 1;

	findType( type, propType, propDim );

	static const property::OfxhPropSpec allString[] = {
		{ kOfxParamPropStringMode, property::eString, 1, false, kOfxParamStringIsSingleLine },
		{ kOfxParamPropStringFilePathExists, property::eInt, 1, false, "1" },
		{ 0 }
	};

	static const property::OfxhPropSpec allChoice[] = {
		{ kOfxParamPropChoiceOption, property::eString, 0, false, "" },
		{ 0 }
	};

	static const property::OfxhPropSpec allCustom[] = {
		{ kOfxParamPropCustomInterpCallbackV1, property::ePointer, 1, false, 0 },
		{ 0 },
	};

	static const property::OfxhPropSpec allPage[] = {
		{ kOfxParamPropPageChild, property::eString, 0, false, "" },
		{ 0 }
	};

	if( propType != property::eNone )
		initValueParamProps( type, propType, propDim );
	else
		initNoValueParamProps();

	if( type == kOfxParamTypeString )
	{
		getEditableProperties().addProperties( allString );
	}

	if( isDoubleParam( type ) || isIntParam( type ) || isColourParam( type ) )
	{
		initNumericParamProps( type, propType, propDim );
	}

	if( type != kOfxParamTypeGroup && type != kOfxParamTypePage )
	{
		initInteractParamProps( type );
	}

	if( type == kOfxParamTypeChoice )
	{
		getEditableProperties().addProperties( allChoice );
	}

	if( type == kOfxParamTypeCustom )
	{
		getEditableProperties().addProperties( allCustom );
	}

	if( type == kOfxParamTypePage )
	{
		getEditableProperties().addProperties( allPage );
	}
}

/**
 * add standard properties to a params that can take an interact
 */
void OfxhParamDescriptor::initInteractParamProps( const std::string& type )
{
	static const property::OfxhPropSpec allButGroupPageProps[] = {
		{ kOfxParamPropInteractV1, property::ePointer, 1, false, 0 },
		{ kOfxParamPropInteractSize, property::eDouble, 2, false, "0" },
		{ kOfxParamPropInteractSizeAspect, property::eDouble, 1, false, "1" },
		{ kOfxParamPropInteractMinimumSize, property::eInt, 2, false, "10" },
		{ kOfxParamPropInteractPreferedSize, property::eInt, 2, false, "10" },
		{ 0 }
	};

	getEditableProperties().addProperties( allButGroupPageProps );
}

/**
 * add standard properties to a value holding param
 */
void OfxhParamDescriptor::initValueParamProps( const std::string& type, property::TypeEnum valueType, int dim )
{
	static const property::OfxhPropSpec invariantProps[] = {
		{ kOfxParamPropAnimates, property::eInt, 1, false, "1" },
		{ kOfxParamPropIsAnimating, property::eInt, 1, false, "0" },
		{ kOfxParamPropIsAutoKeying, property::eInt, 1, false, "0" },
		{ kOfxParamPropPersistant, property::eInt, 1, false, "1" },
		{ kOfxParamPropEvaluateOnChange, property::eInt, 1, false, "1" },
		{ kOfxParamPropPluginMayWrite, property::eInt, 1, false, "0" },
		{ kOfxParamPropCanUndo, property::eInt, 1, false, "1" },
		{ kOfxParamPropCacheInvalidation, property::eString, 1, false, kOfxParamInvalidateValueChange },
		{ 0 }
	};

	property::OfxhPropSpec variantProps[] = {
		{ kOfxParamPropDefault, valueType, dim, false, valueType == property::eString ? "" : "0" },
		{ 0 }
	};

	getEditableProperties().addProperties( invariantProps );
	getEditableProperties().addProperties( variantProps );
}

void OfxhParamDescriptor::initNoValueParamProps()
{
	static const property::OfxhPropSpec invariantProps[] = {
		{ kOfxParamPropAnimates, property::eInt, 1, false, "0" },
		{ kOfxParamPropIsAnimating, property::eInt, 1, false, "0" },
		{ kOfxParamPropIsAutoKeying, property::eInt, 1, false, "0" },
		{ kOfxParamPropPersistant, property::eInt, 1, false, "0" },
		{ kOfxParamPropEvaluateOnChange, property::eInt, 1, false, "0" },
		{ kOfxParamPropPluginMayWrite, property::eInt, 1, false, "0" },
		{ kOfxParamPropCanUndo, property::eInt, 1, false, "0" },
		{ kOfxParamPropCacheInvalidation, property::eString, 1, false, "" },
		{ 0 }
	};

	getEditableProperties().addProperties( invariantProps );
}

/**
 * add standard properties to a value holding param
 */
void OfxhParamDescriptor::initNumericParamProps( const std::string& type, property::TypeEnum valueType, int dim )
{
	static std::string dbl_minstr, dbl_maxstr, int_minstr, int_maxstr;
	bool doneOne = false;

	if( !doneOne )
	{
		// Needed for msvc compilator
		using namespace std;

		std::ostringstream dbl_min, dbl_max, int_min, int_max;
		doneOne = true;
		dbl_min << -numeric_limits<double>::max();
		dbl_max << numeric_limits<double>::max();
		int_min << numeric_limits<int>::min();
		int_max << numeric_limits<int>::max();

		dbl_minstr = dbl_min.str();
		dbl_maxstr = dbl_max.str();
		int_minstr = int_min.str();
		int_maxstr = int_max.str();
	}

	property::OfxhPropSpec allNumeric[] = {
		{ kOfxParamPropDisplayMin, valueType, dim, false, isColourParam( type ) ? "0" : ( valueType == property::eDouble ? dbl_minstr : int_minstr ).c_str() },
		{ kOfxParamPropDisplayMax, valueType, dim, false, isColourParam( type ) ? "1" : ( valueType == property::eDouble ? dbl_maxstr : int_maxstr ).c_str() },
		{ kOfxParamPropMin, valueType, dim, false, ( valueType == property::eDouble ? dbl_minstr : int_minstr ).c_str() },
		{ kOfxParamPropMax, valueType, dim, false, ( valueType == property::eDouble ? dbl_maxstr : int_maxstr ).c_str() },
		{ 0 }
	};

	getEditableProperties().addProperties( allNumeric );

	/// if any double or a colour
	if( valueType == property::eDouble )
	{
		static const property::OfxhPropSpec allDouble[] = {
			{ kOfxParamPropIncrement, property::eDouble, 1, false, "1" },
			{ kOfxParamPropDigits, property::eInt, 1, false, "2" },
			{ 0 }
		};
		getEditableProperties().addProperties( allDouble );
	}

	/// if a double param type
	if( isDoubleParam( type ) )
	{
		static const property::OfxhPropSpec allDouble[] = {
			{ kOfxParamPropDoubleType, property::eString, 1, false, kOfxParamDoubleTypePlain },
			{ 0 }
		};
		getEditableProperties().addProperties( allDouble );

		if( dim == 1 )
		{
			static const property::OfxhPropSpec allDouble1D[] = {
				{ kOfxParamPropShowTimeMarker, property::eInt, 1, false, "0" },
				{ 0 }
			};

			getEditableProperties().addProperties( allDouble1D );
		}
	}

	/// if a multi dimensional param
	if( isDoubleParam( type ) && ( dim == 2 || dim == 3 ) )
	{
		property::OfxhPropSpec all2D3D[] = {
			{ kOfxParamPropDimensionLabel, property::eString, dim, false, "" },
			{ 0 },
		};

		getEditableProperties().addProperties( all2D3D );
		getEditableProperties().setStringProperty( kOfxParamPropDimensionLabel, "X", 0 );
		getEditableProperties().setStringProperty( kOfxParamPropDimensionLabel, "Y", 1 );
		if( dim == 3 )
		{
			getEditableProperties().setStringProperty( kOfxParamPropDimensionLabel, "Z", 2 );
		}
	}

	/// if a multi dimensional param
	if( isColourParam( type ) )
	{
		property::OfxhPropSpec allColor[] = {
			{ kOfxParamPropDimensionLabel, property::eString, dim, false, "" },
			{ 0 },
		};

		getEditableProperties().addProperties( allColor );
		getEditableProperties().setStringProperty( kOfxParamPropDimensionLabel, "R", 0 );
		getEditableProperties().setStringProperty( kOfxParamPropDimensionLabel, "G", 1 );
		getEditableProperties().setStringProperty( kOfxParamPropDimensionLabel, "B", 2 );
		if( dim == 4 )
		{
			getEditableProperties().setStringProperty( kOfxParamPropDimensionLabel, "A", 3 );
		}
	}
}

OfxhParamAccessorSet::~OfxhParamAccessorSet() {}

/// obtain a handle on this set for passing to the C api

OfxhParamDescriptorSet::OfxhParamDescriptorSet() {}

OfxhParamDescriptorSet::~OfxhParamDescriptorSet()
{}

void OfxhParamDescriptorSet::addParam( const std::string& name, OfxhParamDescriptor* p )
{
	_paramList.push_back( p );
	_paramMap[name] = p;
}

/// define a param on this effect

OfxhParamDescriptor* OfxhParamDescriptorSet::paramDefine( const char* paramType,
                                                          const char* name )
{
	if( !isStandardType( paramType ) )
		throw OfxhException( std::string( "The param type '" ) + paramType + "' is not recognize, the param '" + name + "' can't be created." );

	OfxhParamDescriptor* desc = new OfxhParamDescriptor( paramType, name );
	desc->initStandardParamProps( paramType );
	addParam( name, desc );
	return desc;
}

////////////////////////////////////////////////////////////////////////////////
//
// Instance
//

/**
 * the description of a plugin parameter
 */
OfxhParam::~OfxhParam() {}

/**
 * make a parameter, with the given type and name
 */
OfxhParam::OfxhParam( const OfxhParamDescriptor& descriptor, attribute::OfxhParamSet& setInstance )
	: attribute::OfxhAttribute( descriptor ),
	_paramSetInstance( &setInstance ),
	_parentInstance( 0 )
{
	// parameter has to be owned by paramSet
	setInstance.addParam( descriptor.getName(), this ); ///< @todo tuttle move this from here (introduce too many problems), no good reason to be here.

	getEditableProperties().addNotifyHook( kOfxParamPropEnabled, this );
	getEditableProperties().addNotifyHook( kOfxParamPropSecret, this );
	getEditableProperties().addNotifyHook( kOfxPropLabel, this );
}

/**
 * callback which should set enabled state as appropriate
 */
void OfxhParam::setEnabled() {}

/**
 * callback which should set secret state as appropriate
 */
void OfxhParam::setSecret() {}

/**
 * callback which should update label
 */
void OfxhParam::setLabel() {}

/**
 * callback which should set
 */
void OfxhParam::setDisplayRange() {}

/**
 * get a value, implemented by instances to deconstruct var args
 */
void OfxhParam::getV( va_list arg ) OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrUnsupported, std::string("ParamInstance getValue failed (paramName:") + getName() + ")" );
}

/**
 * get a value, implemented by instances to deconstruct var args
 */
void OfxhParam::getV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrUnsupported );
}

/**
 * set a value, implemented by instances to deconstruct var args
 */
void OfxhParam::setV( va_list arg ) OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrUnsupported );
}

/**
 * key a value, implemented by instances to deconstruct var args
 */
void OfxhParam::setV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrUnsupported );
}

/**
 * derive a value, implemented by instances to deconstruct var args
 */
void OfxhParam::deriveV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrUnsupported );
}

/**
 * integrate a value, implemented by instances to deconstruct var args
 */
void OfxhParam::integrateV( OfxTime time1, OfxTime time2, va_list arg ) OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrUnsupported );
}

/**
 * overridden from Property::NotifyHook
 */
void OfxhParam::notify( const std::string& name, bool single, int num ) OFX_EXCEPTION_SPEC
{
	if( name == kOfxPropLabel )
	{
		setLabel();
	}
	if( name == kOfxParamPropEnabled )
	{
		setEnabled();
	}
	if( name == kOfxParamPropSecret )
	{
		setSecret();
	}
	if( name == kOfxParamPropDisplayMin || name == kOfxParamPropDisplayMax )
	{
		setDisplayRange();
	}
}

/**
 * copy one parameter to another
 */
void OfxhParam::copy( const OfxhParam& instance, OfxTime offset ) OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrMissingHostFeature );
}

/**
 * copy one parameter to another, with a range
 */
void OfxhParam::copy( const OfxhParam& instance, OfxTime offset, OfxRangeD range ) OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrMissingHostFeature );
}

void OfxhParam::setParentInstance( OfxhParam* instance )
{
	_parentInstance = instance;
}

OfxhParam* OfxhParam::getParentInstance()
{
	return _parentInstance;
}

//
// KeyframeParam
//

void OfxhKeyframeParam::getNumKeys( unsigned int& nKeys ) const OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrMissingHostFeature );
}

void OfxhKeyframeParam::getKeyTime( int nth, OfxTime& time ) const OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrMissingHostFeature );
}

void OfxhKeyframeParam::getKeyIndex( OfxTime time, int direction, int& index ) const OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrMissingHostFeature );
}

void OfxhKeyframeParam::deleteKey( OfxTime time ) OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrMissingHostFeature );
}

void OfxhKeyframeParam::deleteAllKeys() OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrMissingHostFeature );
}

/**
 * setChildrens have to clone each source instance recursively
 */
void OfxhParamGroup::setChildrens( const OfxhParamSet* childrens )
{
	deleteChildrens();

	/// @todo tuttle: use clone ?
	for( ParamList::const_iterator it = childrens->getParamList().begin(), itEnd = childrens->getParamList().end();
	     it != itEnd;
	     ++it )
	{
		_paramList.push_back( it->clone() );
	}
}

void OfxhParamGroup::addChildren( OfxhParam* children )
{
	children->setParamSetInstance( this );
	_paramList.push_back( children );
}

OfxhParamSet* OfxhParamGroup::getChildrens() const
{
	return (OfxhParamSet*)this;
}

//
// Page Instance
//
const std::map<int, attribute::OfxhParam*>& OfxhParamPage::getChildren() const
{
	// HACK!!!! this really should be done with a notify hook so we don't force
	// _children to be mutable
	if( _children.size() == 0 )
	{
		int nChildren = getProperties().getDimension( kOfxParamPropPageChild );
		for( int i = 0; i < nChildren; i++ )
		{
			std::string childName       = getProperties().getStringProperty( kOfxParamPropPageChild, i );
			attribute::OfxhParam* child = &_paramSetInstance->getParam( childName );
			_children[i] = child;
		}
	}
	return _children;
}

//
// ChoiceInstance
//
/// implementation of var args function

void OfxhParamChoice::getV( va_list arg ) OFX_EXCEPTION_SPEC
{
	int* value = va_arg( arg, int* );

	return get( *value );
}

/**
 * implementation of var args function
 */
void OfxhParamChoice::getV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	int* value = va_arg( arg, int* );

	return get( time, *value );
}

/**
 * implementation of var args function
 */
void OfxhParamChoice::setV( va_list arg ) OFX_EXCEPTION_SPEC
{
	int value = va_arg( arg, int );

	return set( value );
}

/**
 * implementation of var args function
 */
void OfxhParamChoice::setV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	int value = va_arg( arg, int );

	return set( time, value );
}

//
// IntegerInstance
//

void OfxhParamInteger::derive( OfxTime time, int& ) OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrUnsupported );
}

void OfxhParamInteger::integrate( OfxTime time1, OfxTime time2, int& ) OFX_EXCEPTION_SPEC
{
	throw OfxhException( kOfxStatErrUnsupported );
}

/**
 * implementation of var args function
 */
void OfxhParamInteger::getV( va_list arg ) OFX_EXCEPTION_SPEC
{
	int* value = va_arg( arg, int* );

	return get( *value );
}

/**
 * implementation of var args function
 */
void OfxhParamInteger::getV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	int* value = va_arg( arg, int* );

	return get( time, *value );
}

/**
 * implementation of var args function
 */
void OfxhParamInteger::setV( va_list arg ) OFX_EXCEPTION_SPEC
{
	int value = va_arg( arg, int );

	return set( value );
}

/**
 * implementation of var args function
 */
void OfxhParamInteger::setV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	int value = va_arg( arg, int );

	return set( time, value );
}

/**
 * implementation of var args function
 */
void OfxhParamInteger::deriveV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	int* value = va_arg( arg, int* );

	return derive( time, *value );
}

/**
 * implementation of var args function
 */
void OfxhParamInteger::integrateV( OfxTime time1, OfxTime time2, va_list arg ) OFX_EXCEPTION_SPEC
{
	int* value = va_arg( arg, int* );

	return integrate( time1, time2, *value );
}

//
// DoubleInstance
//

/**
 * implementation of var args function
 */
void OfxhParamDouble::getV( va_list arg ) OFX_EXCEPTION_SPEC
{
	double* value = va_arg( arg, double* );

	return get( *value );
}

/**
 * implementation of var args function
 */
void OfxhParamDouble::getV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	double* value = va_arg( arg, double* );

	return get( time, *value );
}

/**
 * implementation of var args function
 */
void OfxhParamDouble::setV( va_list arg ) OFX_EXCEPTION_SPEC
{
	double value = va_arg( arg, double );

	return set( value );
}

/**
 * implementation of var args function
 */
void OfxhParamDouble::setV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	double value = va_arg( arg, double );

	return set( time, value );
}

/**
 * implementation of var args function
 */
void OfxhParamDouble::deriveV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	double* value = va_arg( arg, double* );

	return derive( time, *value );
}

/**
 * implementation of var args function
 */
void OfxhParamDouble::integrateV( OfxTime time1, OfxTime time2, va_list arg ) OFX_EXCEPTION_SPEC
{
	double* value = va_arg( arg, double* );

	return integrate( time1, time2, *value );
}

//
// BooleanInstance
//

/**
 * implementation of var args function
 */
void OfxhParamBoolean::getV( va_list arg ) OFX_EXCEPTION_SPEC
{
	bool v;
	get( v );

	int* value = va_arg( arg, int* );
	*value = v;
}

/**
 * implementation of var args function
 */
void OfxhParamBoolean::getV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	bool v;
	get( time, v );

	int* value = va_arg( arg, int* );
	*value = v;
}

/**
 * implementation of var args function
 */
void OfxhParamBoolean::setV( va_list arg ) OFX_EXCEPTION_SPEC
{
	bool value = va_arg( arg, int ) != 0;
	set( value );
}

/**
 * implementation of var args function
 */
void OfxhParamBoolean::setV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	bool value = va_arg( arg, int ) != 0;
	set( time, value );
}

////////////////////////////////////////////////////////////////////////////////
// string param

void OfxhParamString::getV( va_list arg ) OFX_EXCEPTION_SPEC
{
	const char** value = va_arg( arg, const char** );
	get( _returnValue ); /// @todo tuttle: "I so don't like this, temp storage should be delegated to the implementation"

	*value = _returnValue.c_str();
}

/**
 * implementation of var args function
 */
void OfxhParamString::getV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	const char** value = va_arg( arg, const char** );
	get( time, _returnValue ); // I so don't like this, temp storage should be delegated to the implementation

	*value = _returnValue.c_str();
}

/**
 * implementation of var args function
 */
void OfxhParamString::setV( va_list arg ) OFX_EXCEPTION_SPEC
{
	char* value = va_arg( arg, char* );
	set( value );
}

/**
 * implementation of var args function
 */
void OfxhParamString::setV( OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	char* value = va_arg( arg, char* );
	set( time, value );
}

//////////////////////////////////////////////////////////////////////////////////
// ParamInstanceSet
//

OfxhParamSet::OfxhParamSet()
{}

OfxhParamSet::OfxhParamSet( const OfxhParamSet& other )
{
	operator=( other );
}

void OfxhParamSet::initMapFromList()
{
	for( ParamList::iterator it = _paramList.begin(), itEnd = _paramList.end();
	     it != itEnd;
	     ++it )
	{
		_params[it->getName()] = &( *it );
	}
}

OfxhParamSet::~OfxhParamSet()
{}

void OfxhParamSet::operator=( const OfxhParamSet& other )
{
	_paramList = other._paramList.clone();
	initMapFromList();
}

void OfxhParamSet::copyParamsValues( const OfxhParamSet& other )
{
	if( _paramList.size() != other._paramList.size() )
		throw core::exception::LogicError( "You try to copy parameters values, but the two lists are not identical." );

	ParamList::const_iterator oit = other._paramList.begin(), oitEnd = other._paramList.end();
	for( ParamList::iterator it = _paramList.begin(), itEnd = _paramList.end();
	     it != itEnd && oit != oitEnd;
	     ++it, ++oit )
	{
		OfxhParam& p = *it;
		const OfxhParam& op = *oit;
		if( p.getName() != op.getName() )
			throw core::exception::LogicError( "You try to copy parameters values, but it is not the same parameters in the two lists." );
		p.copy(op);
	}
	initMapFromList();
}

void OfxhParamSet::addParam( const std::string& name, OfxhParam* instance ) OFX_EXCEPTION_SPEC
{
	if( _params.find( name ) != _params.end() )
		throw OfxhException( kOfxStatErrExists );
	_params[name] = instance;
	_paramList.push_back( instance );
}

}
}
}
}
