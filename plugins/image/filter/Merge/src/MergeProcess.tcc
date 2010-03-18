#include "gil/toolbox/hsl.hpp"
#include "gil/toolbox/ViewsMerging.hpp"
#include <tuttle/common/math/rectOp.hpp>
#include <tuttle/plugin/ImageGilProcessor.hpp>
#include <tuttle/plugin/PluginException.hpp>

#include <cstdlib>
#include <cassert>
#include <cmath>
#include <vector>
#include <iostream>
#include <ofxsImageEffect.h>
#include <ofxsMultiThread.h>
#include <boost/gil/gil_all.hpp>

namespace tuttle {
namespace plugin {
namespace merge {

using namespace boost::gil;

template<class View, class Functor>
MergeProcess<View, Functor>::MergeProcess( MergePlugin& instance )
	: ImageGilProcessor<View>( instance ),
	_plugin( instance )
{}

template<class View, class Functor>
void MergeProcess<View, Functor>::setup( const OFX::RenderArguments& args )
{
	// sources view
	// clip A
	_srcA.reset( _plugin.getSrcClipA()->fetchImage( args.time ) );
	if( !_srcA.get() )
		throw( ImageNotReadyException() );
	if( _srcA->getRowBytes( ) <= 0 )
		throw( WrongRowBytesException( ) );
	this->_srcViewA = this->getView( _srcA.get(), _plugin.getSrcClipA()->getPixelRod(args.time) );
	// clip B
	_srcB.reset( _plugin.getSrcClipB()->fetchImage( args.time ) );
	if( !_srcB.get() )
		throw( ImageNotReadyException() );
	if( _srcB->getRowBytes( ) <= 0 )
		throw( WrongRowBytesException( ) );
	this->_srcViewB = this->getView( _srcB.get(), _plugin.getSrcClipB()->getPixelRod(args.time) );
	// destination view
	_dst.reset( _plugin.getDstClip()->fetchImage( args.time ) );
	if( !_dst.get() )
		throw( ImageNotReadyException() );
	if( _dst->getRowBytes( ) <= 0 )
		throw( WrongRowBytesException( ) );
	this->_dstView = this->getView( _dst.get(), _plugin.getDstClip()->getPixelRod(args.time) );

	// Make sure bit depths are the same
	if( _srcA->getPixelDepth() != _dst->getPixelDepth() ||
	    _srcB->getPixelDepth() != _dst->getPixelDepth() ||
		_srcA->getPixelComponents() != _dst->getPixelComponents() ||
	    _srcB->getPixelComponents() != _dst->getPixelComponents() )
	{
		throw( BitDepthMismatchException() );
	}

}

/**
 * @brief Function called by rendering thread each time
 *        a process must be done.
 *
 * @param[in] procWindow  Processing window
 */
template<class View, class Functor>
void MergeProcess<View, Functor>::multiThreadProcessImages( const OfxRectI& procWindow )
{
	using namespace hsl_color_space;
	View _srcA = subimage_view( this->_srcViewA,
							   procWindow.x1 - this->_renderWindow.x1,
							   procWindow.y1 - this->_renderWindow.y1,
							   procWindow.x2 - procWindow.x1,
							   procWindow.y2 - procWindow.y1 );
	View _srcB = subimage_view( this->_srcViewB,
							   procWindow.x1 - this->_renderWindow.x1,
							   procWindow.y1 - this->_renderWindow.y1,
							   procWindow.x2 - procWindow.x1,
							   procWindow.y2 - procWindow.y1 );
	View _dst = subimage_view( this->_dstView,
							  procWindow.x1 - this->_renderWindow.x1,
							  procWindow.y1 - this->_renderWindow.y1,
							  procWindow.x2 - procWindow.x1,
							  procWindow.y2 - procWindow.y1 );
	merge_pixels( _srcA, _srcB, _dst, Functor() );
}

}
}
}