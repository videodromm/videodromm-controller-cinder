#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "MPEApp.hpp"
#include "MPEClient.h"

// UserInterface
#include "CinderImGui.h"
// Warping
#include "Warp.h"
// Settings
#include "VDSettings.h"
// Session
#include "VDSession.h"
// Log
#include "VDLog.h"
// Utils
#include "VDUtils.h"
// Message router
#include "VDRouter.h"
// Animation
#include "VDAnimation.h"
// Image sequence
#include "VDImageSequence.h"
// UnionJack
#include "UnionJack.h"
// spout
#include "spout.h"

using namespace ci;
using namespace ci::app;
using namespace ph::warping;
using namespace mpe;
//using namespace Videodromm;

#define IM_ARRAYSIZE(_ARR)			((int)(sizeof(_ARR)/sizeof(*_ARR)))

class VideodrommControllerApp : public App, public MPEApp
{
public:
	void setup();
	void mpeReset();

	void update();
	void mpeFrameUpdate(long serverFrameNumber);

	void draw();
	void mpeFrameRender(bool isNewFrame);

	MPEClientRef mClient;
};
