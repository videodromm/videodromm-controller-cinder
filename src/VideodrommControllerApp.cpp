#include "VideodrommControllerApp.h"

/*
TODO
- 2802 imgui contextual window depending on mouseover
- 2802 imgui vertical : textures
- flip horiz
- check flip H and V (spout also)
- sort fbo names and indexes (warps only 4 or 5 inputs)
- spout texture 10 create shader 10.glsl(ThemeFromBrazil) iChannel0
- warpwrapper handle texture mode 0 for spout (without fbo)
- put sliderInt instead of popups //warps next
- proper slitscan h and v //wip
- proper rotation
- badtv in mix.frag

tempo 142
bpm = (60 * fps) / fpb

where bpm = beats per min
fps = frames per second
fpb = frames per beat

fpb = 4, bpm = 142
fps = 142 / 60 * 4 = 9.46
*/

void VideodrommControllerApp::prepare(Settings *settings)
{
	settings->setWindowSize(40, 10);
	settings->setBorderless();
}
void VideodrommControllerApp::setup()
{

	// Log
	mVDLog = VDLog::create();
	CI_LOG_V("Controller");
	// Settings
	mVDSettings = VDSettings::create();
	mVDSettings->mLiveCode = false;
	//mVDSettings->mRenderThumbs = false;
	// Session
	mVDSession = VDSession::create(mVDSettings);
	// Utils
	//mVDUtils = VDUtils::create(mVDSettings);
	// Animation
	mVDAnimation = VDAnimation::create(mVDSettings, mVDSession);
	// Message router
	mVDRouter = VDRouter::create(mVDSettings, mVDAnimation, mVDSession);

	// Mix
	mMixesFilepath = getAssetPath("") / "mixes.xml";
	if (fs::exists(mMixesFilepath)) {
		// load textures from file if one exists
		mMixes = VDMix::readSettings(mVDSettings, mVDAnimation, mVDRouter, loadFile(mMixesFilepath));
	}
	else {
		// otherwise create a texture from scratch
		mMixes.push_back(VDMix::create(mVDSettings, mVDAnimation, mVDRouter));
	}

	setFrameRate(mVDSession->getTargetFps());
	mMovieDelay = mFadeInDelay = true;
	mVDAnimation->tapTempo();
	mVDSession->getWindowsResolution();
	// UI
	mVDUI = VDUI::create(mVDSettings, mMixes[0], mVDRouter, mVDAnimation, mVDSession);

	getWindow()->getSignalResize().connect(std::bind(&VideodrommControllerApp::resizeWindow, this));
	getWindow()->getSignalDraw().connect(std::bind(&VideodrommControllerApp::drawRenderWindow, this));

	if (mVDSettings->mStandalone) {

	}
	else {

		// OK mControlWindow = createWindow(Window::Format().size(mVDSettings->mMainWindowWidth, mVDSettings->mMainWindowHeight));
		mControlWindow = createWindow(Window::Format().size(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight));
		mControlWindow->setPos(mVDSettings->mMainWindowX, mVDSettings->mMainWindowY);
		//mControlWindow->setBorderless();
		mControlWindow->getSignalDraw().connect(std::bind(&VideodrommControllerApp::drawControlWindow, this));
		mControlWindow->getSignalResize().connect(std::bind(&VideodrommControllerApp::resizeWindow, this));
	}

	// mouse cursor and ui
	setUIVisibility(mVDSettings->mCursorVisible);
	// maximize fps
	disableFrameRate();
	gl::enableVerticalSync(false);
}
void VideodrommControllerApp::cleanup()
{
	CI_LOG_V("shutdown");
	mMixes[0]->save();
	mVDSettings->save();
	mVDSession->save();
	ui::Shutdown();
	quit();
}

void VideodrommControllerApp::resizeWindow()
{
	mVDUI->resize();
	if (mVDSettings->mStandalone) {
		// set ui window and io events callbacks
		ui::disconnectWindow(getWindow());
	}
	else {
		ui::disconnectWindow(mControlWindow);
	}
	mMixes[0]->resize();

}

void VideodrommControllerApp::mouseMove(MouseEvent event)
{
	if (!mMixes[0]->handleMouseMove(event)) {
		// let your application perform its mouseMove handling here
	}
}

void VideodrommControllerApp::mouseDown(MouseEvent event)
{
	if (!mMixes[0]->handleMouseDown(event)) {
		// let your application perform its mouseDown handling here
	}
}

void VideodrommControllerApp::mouseDrag(MouseEvent event)
{
	if (!mMixes[0]->handleMouseDrag(event)) {
		// let your application perform its mouseDrag handling here
	}
}

void VideodrommControllerApp::mouseUp(MouseEvent event)
{
	if (!mMixes[0]->handleMouseUp(event)) {
		// let your application perform its mouseUp handling here
	}
}

void VideodrommControllerApp::keyDown(KeyEvent event)
{
	if (!mMixes[0]->handleKeyDown(event)) {
		switch (event.getCode()) {
		case KeyEvent::KEY_ESCAPE:
			// quit the application
			quit();
			break;
		case KeyEvent::KEY_h:
			// mouse cursor and ui visibility
			mVDSettings->mCursorVisible = !mVDSettings->mCursorVisible;
			setUIVisibility(mVDSettings->mCursorVisible);
			break;
		}
	}
}
void VideodrommControllerApp::keyUp(KeyEvent event)
{
	if (!mMixes[0]->handleKeyUp(event)) {
	}
}
void VideodrommControllerApp::setUIVisibility(bool visible)
{
	if (visible)
	{
		showCursor();
	}
	else
	{
		hideCursor();
	}
}

void VideodrommControllerApp::update()
{
	mVDSettings->iFps = getAverageFps();
	mVDSettings->sFps = toString(floor(mVDSettings->iFps));
	mMixes[0]->update();
	mVDAnimation->update();
	mVDRouter->update();
	// check if a shader has been received from websockets
	if (mVDSettings->mShaderToLoad != "") {
		mMixes[0]->loadFboFragmentShader(mVDSettings->mShaderToLoad, 1);
	}
}
void VideodrommControllerApp::fileDrop(FileDropEvent event)
{
	int index = (int)(event.getX() / (mVDSettings->uiElementWidth + mVDSettings->uiMargin));// +1;
	ci::fs::path mPath = event.getFile(event.getNumFiles() - 1);
	string mFile = mPath.string();
	if (mMixes[0]->loadFileFromAbsolutePath(mFile, index) > -1) {
		
	}
}

void VideodrommControllerApp::drawRenderWindow()
{
	getWindow()->setTitle("(" + mVDSettings->sFps + " fps) " + toString(mVDSettings->iBeat) + " Videodromm");
	//renderSceneToFbo();
	if (mFadeInDelay) {
		if (getElapsedFrames() > mVDSession->getFadeInDelay()) {
			mFadeInDelay = false;
			setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);
			setWindowPos(ivec2(mVDSettings->mRenderX, mVDSettings->mRenderY));
			timeline().apply(&mVDSettings->iAlpha, 0.0f, 1.0f, 1.5f, EaseInCubic());
		}
	}
	/*if (mMovieDelay) {
		if (getElapsedFrames() > mVDSession->getMoviePlaybackDelay()) {
		mMovieDelay = false;
		fs::path movieFile = getAssetPath("") / mVDSettings->mAssetsPath / mVDSession->getMovieFileName();
		mVDTextures->loadMovie(0, movieFile.string());
		}
		}*/
	gl::clear(Color::black());
	gl::setMatricesWindow(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, false);

	gl::draw(mMixes[0]->getRenderTexture(), getWindowBounds());
}

void VideodrommControllerApp::drawControlWindow()
{

	gl::clear(Color::black());
	//gl::color(Color::white());
	gl::setMatricesWindow(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, true);
	gl::draw(mMixes[0]->getMixTexture(), Rectf(10 + mVDSettings->uiLargeW, 170, 650 + mVDSettings->uiLargeW, 650));
	// imgui
	mVDUI->Run("UI", (int)getAverageFps());
	if (mVDUI->isReady()) {
	}
}

CINDER_APP(VideodrommControllerApp, RendererGl, &VideodrommControllerApp::prepare)
