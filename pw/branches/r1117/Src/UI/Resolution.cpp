#include "stdafx.h"
#include "Resolution.h"

#include "Render/renderer.h"
#include "Render/uirenderer.h"
#include "Render/AABB.h"

#include "NivalInput/InputEvent.h"


namespace UI
{

static Point g_uiScreenResolution( 0, 0 );
static Point g_screenResolution( 0, 0 );

// User UI scale: the virtual UI resolution is divided by this, so values > 1
// render the whole interface proportionally larger.
// 0 (the default) = auto: stock 1.0 on 16:9 and narrower, growing with the
// monitor aspect up to 1.25 on 32:9, so ultrawide players get a comfortable
// HUD out of the box without touching any settings. Values 0.5..1.25 pin the
// scale manually. The editor path always stays at the stock 1280x1024 space.
// Upper bound 1.25: above it the virtual height drops below the tallest authored
// centered dialogs (~835 of 1024) and their edges/buttons start to clip.
static float s_uiScale = 0.0f;
static float g_appliedUiScale = 0.0f;
static bool g_editorMode = false;

// Accepts 0 (= auto) or a fixed scale in [0.5 .. 1.25]
struct UiScaleVerifyPred : NGlobal::StdVerifyPred<float>
{
	bool operator()( const NGlobal::VariantValue &val ) const
	{
		if ( !NGlobal::StdVerifyPred<float>::operator()( val ) )
			return false;
		const float v = val.Get<float>();
		return v == 0.0f || ( v >= 0.5f && v <= 1.25f );
	}
};
REGISTER_VAR_PRED( "ui_scale", s_uiScale, STORAGE_USER, UiScaleVerifyPred() );


void UpdateScreenResolution( const int width, const int height, const bool editor )
{
	g_screenResolution.x = width;
	g_screenResolution.y = height;

	g_uiScreenResolution.x = UI_RESOLUTION_WIDTH;
	g_uiScreenResolution.y = UI_RESOLUTION_HEIGHT;

	if ( width * UI_RESOLUTION_HEIGHT > height * UI_RESOLUTION_WIDTH )
		g_uiScreenResolution.x = width * UI_RESOLUTION_HEIGHT / height;
	else
		g_uiScreenResolution.y = height * UI_RESOLUTION_WIDTH / width;

	g_editorMode = editor;
	if ( !editor )
	{
		g_appliedUiScale = s_uiScale;
		float scale = s_uiScale;
		if ( scale == 0.0f )
		{
			// auto: comfort scale from the monitor aspect - stock 1.0 at 16:9 and
			// narrower, growing with the relative width surplus up to 1.25 at 32:9
			const float baseAspect = 16.0f / 9.0f;
			const float aspect = (float)width / (float)height;
			scale = aspect <= baseAspect ? 1.0f : 1.0f + 0.25f * ( aspect / baseAspect - 1.0f );
			if ( scale > 1.25f )
				scale = 1.25f;
			if ( scale < 1.01f ) // snap near-16:9 panels (1366x768 etc.) to stock
				scale = 1.0f;
		}
		if ( scale != 1.0f )
		{
			g_uiScreenResolution.x = (int)( g_uiScreenResolution.x / scale + 0.5f );
			g_uiScreenResolution.y = (int)( g_uiScreenResolution.y / scale + 0.5f );
		}
	}

	if ( !editor )
		Render::GetUIRenderer()->SetResolutionCoefs( 2.0f / g_uiScreenResolution.x, 2.0f / g_uiScreenResolution.y,
                              static_cast<float>(g_screenResolution.x) / static_cast<float>(g_uiScreenResolution.x),
                              static_cast<float>(g_screenResolution.y) / static_cast<float>(g_uiScreenResolution.y) );
	else
	{
		const Render::RenderMode& renderMode = Render::GetRenderer()->GetCurrentRenderMode();
		const float fixedWidth = renderMode.width * g_uiScreenResolution.x / g_screenResolution.x;
		const float fixedHeight = renderMode.height * g_uiScreenResolution.y / g_screenResolution.y;
		Render::GetUIRenderer()->SetResolutionCoefs( 2.0f / fixedWidth, 2.0f / fixedHeight,
                              static_cast<float>(g_screenResolution.x) / static_cast<float>(g_uiScreenResolution.x),
                              static_cast<float>(g_screenResolution.y) / static_cast<float>(g_uiScreenResolution.y) );
	}
}



void RefreshUIScaleIfChanged()
{
	if ( g_editorMode || g_appliedUiScale == s_uiScale )
		return;
	if ( g_screenResolution.x <= 0 || g_screenResolution.y <= 0 )
		return;

	UpdateScreenResolution( g_screenResolution.x, g_screenResolution.y, false );
}



const Point& GetUIScreenResolution()
{
	return g_uiScreenResolution;
}



const Point& GetScreenResolution()
{
  return g_screenResolution;
}



float GetWindowPlacement( Point * result, LinearCoord * resultSize, const CVec3 & posWorld, float size, const SHMatrix & projectionMatrix )
{
  const float uw = UI::GetUIScreenResolution().x;
  const float uh = UI::GetUIScreenResolution().y;

  CVec4 projected( 0, 0, 0, 0 );
  projectionMatrix.RotateHVector( &projected, CVec4( posWorld, 1.0f ) );

  result->x = (LinearCoord)( 0.5f + ( projected.x / projected.w + 1.0f ) * 0.5f * uw );
  result->y = (LinearCoord)( 0.5f + uh - ( projected.y / projected.w + 1.0f ) * 0.5f * uh );

  if ( resultSize )
  {
    SHMatrix inv;
    inv.HomogeneousInverse( projectionMatrix ); //FIXME: only first matrix column is used
    CVec3 ox( inv.xx, inv.yx, inv.zx );
    Normalize( &ox );
    ox *= size;

    CVec4 projDir;
    projectionMatrix.RotateHDirection( &projDir, ox ); //FIXME: 'x' and 'z' are unused and calculated in vain
    *resultSize = (LinearCoord)fabs( 0.5f + projDir.x / projected.w * 0.5f * uw );
  }

  return projected.w;
}



float GetWindowPlacement( Rect * result, Render::AABB const & aabb, const SHMatrix & projectionMatrix )
{
  const LinearCoord uw = UI::GetUIScreenResolution().x;
  const LinearCoord uh = UI::GetUIScreenResolution().y;

  const int numPoints = 8;
  CVec3 p1 = aabb.center - aabb.halfSize, p2 = aabb.center + aabb.halfSize;
  CVec3 p[ numPoints ] = { p1, CVec3( p1.x, p1.y, p2.z ), CVec3( p1.x, p2.y, p1.z ), CVec3( p1.x, p2.y, p2.z ),
                               CVec3( p2.x, p1.y, p1.z ), CVec3( p2.x, p1.y, p2.z ), CVec3( p2.x, p2.y, p1.z ), p2 };

  float depth = 0;
  for ( int i = 0; i < numPoints; ++i )
  {
    CVec4 pos( p[ i ], 1.0f ), transPos;
    projectionMatrix.RotateHVector( &transPos, pos );

    float x = ( transPos.x / transPos.w + 1.0 ) * 0.5 * uw;
    float y = uh - ( transPos.y / transPos.w + 1.0 ) * 0.5 * uh;

    if ( i )
      depth = Min( depth, transPos.w );
    else
      depth = transPos.w;

    if(i)
    {
      result->minx = Min( result->minx, (LinearCoord)x );
      result->maxx = Max( result->maxx, (LinearCoord)x );

      result->miny = Min( result->miny, (LinearCoord)y );
      result->maxy = Max( result->maxy, (LinearCoord)y );
    }
    else
      result->Set( x, y, x, y );
  }

	return depth;
}



const float GetUIScreenRatio()
{
	if ( g_uiScreenResolution.x == 0 )
		return 0.0f;
	else
		return (float)g_screenResolution.x / (float)g_uiScreenResolution.x;
}



Point ConvertToScreen( const Point & ui )
{
  if ( !g_uiScreenResolution.x || !g_uiScreenResolution.y )
    return Point( 0, 0 );

  return Point(
    ui.x * g_screenResolution.x / g_uiScreenResolution.x, 
    ui.y * g_screenResolution.y / g_uiScreenResolution.y );
}



Point ConvertFromScreen( const Point & ui )
{
  if ( !g_screenResolution.x || !g_screenResolution.y )
    return Point( 0, 0 );

  return Point(
    ui.x * g_uiScreenResolution.x / g_screenResolution.x, 
    ui.y * g_uiScreenResolution.y / g_screenResolution.y );
}



float SnapToScreenPixelX( float uiCoordX, unsigned options )
{
  float screen = uiCoordX / g_uiScreenResolution.x * g_screenResolution.x;
  if ( options & ESnapOptions::RoundOff )
    screen += 0.5f;
  screen = floorf( screen );
  return screen / g_screenResolution.x * g_uiScreenResolution.x;
}



float SnapToScreenPixelY( float uiCoordY, unsigned options )
{
  float screen = uiCoordY / g_uiScreenResolution.y * g_screenResolution.y;
  if ( options & ESnapOptions::RoundOff )
    screen += 0.5f;
  screen = floorf( screen );
  return screen / g_screenResolution.y * g_uiScreenResolution.y;
}



void SnapRectToScreenPixels( Rect & rect, unsigned options )
{
  const LinearCoord width = rect.Width(), height = rect.Height();

  rect.left   = SnapToScreenPixelX( rect.left, options );
  rect.top    = SnapToScreenPixelY( rect.top, options );

  if ( !( options & ESnapOptions::KeepRight ) )
  {
    if ( options & ESnapOptions::PreserveWidth )
      rect.right = rect.left + width;
    else
      rect.right = SnapToScreenPixelX( rect.right, options );
  }

  if ( !( options & ESnapOptions::KeepBottom ) )
  {
    if ( options & ESnapOptions::PreserveHeight )
      rect.bottom = rect.top + height;
    else
      rect.bottom = SnapToScreenPixelY( rect.bottom, options );
  }
}



Point ExtractMsgCoords( const Input::SSysParams & eventSysParams )
{
  CVec2 pos( eventSysParams.GetMouseX(), eventSysParams.GetMouseY() );
  Point coords;
	coords.x = ( pos.x * g_uiScreenResolution.x ) / g_screenResolution.x;
	coords.y = ( pos.y * g_uiScreenResolution.y ) / g_screenResolution.y;
	return coords;
}

} //namespace UI
