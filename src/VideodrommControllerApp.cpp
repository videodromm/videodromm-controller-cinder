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
	CI_LOG_V("Controller");
	// Settings
	mVDSettings = VDSettings::create();
	// Session
	mVDSession = VDSession::create(mVDSettings);

	setFrameRate(mVDSession->getTargetFps());
	mMovieDelay = mFadeInDelay = true;
	mVDSession->getWindowsResolution();

	getWindow()->getSignalResize().connect(std::bind(&VideodrommControllerApp::resizeWindow, this));
	getWindow()->getSignalDraw().connect(std::bind(&VideodrommControllerApp::drawRenderWindow, this));

	// mouse cursor and ui
	setUIVisibility(mVDSettings->mCursorVisible);
	// UI
	mVDUI = VDUI::create(mVDSettings, mVDSession);

	setFrameRate(mVDSession->getTargetFps());
	CI_LOG_V("setup");
}
void VideodrommControllerApp::createControlWindow()
{
	mVDUI->resize();

	deleteControlWindows();
	mVDSession->getWindowsResolution();
	CI_LOG_V("createRenderWindow, resolution:" + toString(mVDSettings->mRenderWidth) + "x" + toString(mVDSettings->mRenderHeight));

	string windowName = "render";
	// OK mControlWindow = createWindow(Window::Format().size(mVDSettings->mMainWindowWidth, mVDSettings->mMainWindowHeight));
	mControlWindow = createWindow(Window::Format().size(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight));
	mControlWindow->setPos(mVDSettings->mMainWindowX, mVDSettings->mMainWindowY);
	//mControlWindow->setBorderless();
	mControlWindow->getSignalDraw().connect(std::bind(&VideodrommControllerApp::drawControlWindow, this));
	mControlWindow->getSignalResize().connect(std::bind(&VideodrommControllerApp::resizeWindow, this));

	// create instance of the window and store in vector
	allWindows.push_back(mControlWindow);
	mVDSettings->mRenderPosXY = ivec2(mVDSettings->mRenderX, mVDSettings->mRenderY);
}

void VideodrommControllerApp::deleteControlWindows()
{
#if defined( CINDER_MSW )
	for (auto wRef : allWindows) DestroyWindow((HWND)mControlWindow->getNative());
#endif
	allWindows.clear();
}
void VideodrommControllerApp::cleanup()
{
	CI_LOG_V("shutdown");
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
}

void VideodrommControllerApp::mouseMove(MouseEvent event)
{
	if (!mVDSession->handleMouseMove(event)) {
		// let your application perform its mouseMove handling here
	}
}

void VideodrommControllerApp::mouseDown(MouseEvent event)
{
	if (!mVDSession->handleMouseDown(event)) {
		// let your application perform its mouseDown handling here
	}
}

void VideodrommControllerApp::mouseDrag(MouseEvent event)
{
	if (!mVDSession->handleMouseDrag(event)) {
		// let your application perform its mouseDrag handling here
	}
}

void VideodrommControllerApp::mouseUp(MouseEvent event)
{
	if (!mVDSession->handleMouseUp(event)) {
		// let your application perform its mouseUp handling here
	}
}

void VideodrommControllerApp::keyDown(KeyEvent event)
{
	if (!mVDSession->handleKeyDown(event)) {
		switch (event.getCode()) {
		case KeyEvent::KEY_KP_PLUS:
			createControlWindow();
			break;
		case KeyEvent::KEY_KP_MINUS:
			deleteControlWindows();
			break;
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
	if (!mVDSession->handleKeyUp(event)) {
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
	switch (mVDSession->getCmd()) {
	case 0:
		createControlWindow();
		break;
	case 1:
		deleteControlWindows();
		break;
	}
	mVDSession->setFloatUniformValueByIndex(mVDSettings->IFPS, getAverageFps());
	mVDSession->update();
	/* obsolete check if a shader has been received from websockets
	if (mVDSettings->mShaderToLoad != "") {
	mVDSession->loadFboFragmentShader(mVDSettings->mShaderToLoad, 1);
	}*/
}
void VideodrommControllerApp::fileDrop(FileDropEvent event)
{
	mVDSession->fileDrop(event);
}

void VideodrommControllerApp::drawRenderWindow()
{
	getWindow()->setTitle("(" + mVDSettings->sFps + " fps) " + toString(mVDSettings->iBeat) + " Videodromm");
	//renderSceneToFbo();
	if (mFadeInDelay) {
		mVDSettings->iAlpha = 0.0f;
		if (getElapsedFrames() > mVDSession->getFadeInDelay()) {
			mFadeInDelay = false;
			setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);
			setWindowPos(ivec2(mVDSettings->mRenderX, mVDSettings->mRenderY));
			// warps resize at the end
			mVDSession->resize();
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
	gl::draw(mVDSession->getRenderTexture(), getWindowBounds());
}

void VideodrommControllerApp::drawControlWindow()
{
	gl::clear(Color::black());
	//gl::color(Color::white());
	gl::setMatricesWindow(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, false);
	gl::draw(mVDSession->getMixTexture(mVDSession->getCurrentEditIndex()), Rectf(10 + mVDSettings->uiLargeW, 170, 650 + mVDSettings->uiLargeW, 650));
	//gl::draw(mVDSession->getMixTexture(), Rectf(0, 170, 350 , 350));
	if (mVDSettings->mCursorVisible) {
		// imgui
		mVDUI->Run("UI", (int)getAverageFps());
		if (mVDUI->isReady()) {
		}

	}
}

CINDER_APP(VideodrommControllerApp, RendererGl, &VideodrommControllerApp::prepare)
