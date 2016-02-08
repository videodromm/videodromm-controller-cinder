#include "VideodrommControllerApp.h"

void VideodrommControllerApp::setup()
{
    mClient = MPEClient::create(this);
}

void VideodrommControllerApp::mpeReset()
{
    // Reset the state of your app.
    // This will be called when any client connects.
}

void VideodrommControllerApp::update()
{
    if (!mClient->isConnected() && (getElapsedFrames() % 60) == 0)
    {
        mClient->start();
    }
}

void VideodrommControllerApp::mpeFrameUpdate(long serverFrameNumber)
{
    // Your update code.
}

void VideodrommControllerApp::draw()
{
    mClient->draw();
}

void VideodrommControllerApp::mpeFrameRender(bool isNewFrame)
{
    gl::clear(Color(0.5,0.5,0.5));
    // Your render code.
}

// If you're deploying to iOS, set the Render antialiasing to 0 for a significant
// performance improvement. This value defaults to 4 (AA_MSAA_4) on iOS and 16 (AA_MSAA_16)
// on the Desktop.
#if defined( CINDER_COCOA_TOUCH )
CINDER_APP( VideodrommControllerApp, RendererGl(RendererGl::AA_NONE) )
#else
CINDER_APP( VideodrommControllerApp, RendererGl )
#endif
