#include "VideodrommControllerApp.h"

/*
TODO
- 2802 texture class isSequence
- 2802 list of shaders show/active on mouseover
- 2802 imgui contextual window depending on mouseover
- 2802 imgui vertical : textures
- warp select mix fbo texture
- flip horiz
- check flip H and V (spout also)
- sort fbo names and indexes (warps only 4 or 5 inputs)
- spout texture 10 create shader 10.glsl(ThemeFromBrazil) iChannel0
- warpwrapper handle texture mode 0 for spout (without fbo)
- put sliderInt instead of popups //warps next
- proper slitscan h and v //wip
- proper rotation
- badtv in mix.frag
- blendmode preview

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
	mVDUtils = VDUtils::create(mVDSettings);
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
	mVDUtils->getWindowsResolution();
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

	// warping
	//gl::enableDepthRead();
	//gl::enableDepthWrite();
	// fonts ?
	//gl::enableAlphaBlending(false);
	// initialize warps
	mWarpSettings = getAssetPath("") / mVDSettings->mAssetsPath / "warps.xml";
	if (fs::exists(mWarpSettings)) {
		// load warp settings from file if one exists
		mWarps = Warp::readSettings(loadFile(mWarpSettings));
	}
	else {
		// otherwise create a warp from scratch
		mWarps.push_back(WarpPerspectiveBilinear::create());
	}

	//Warp::setSize(mWarps, ivec2(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight));
	Warp::setSize(mWarps, ivec2(mVDSettings->mFboWidth, mVDSettings->mFboHeight));
	mWarpFboIndex = 0;

	mSaveThumbTimer = 0.0f;

	// mouse cursor and ui
	setUIVisibility(mVDSettings->mCursorVisible);
	// maximize fps
	disableFrameRate();
	gl::enableVerticalSync(false);
}
void VideodrommControllerApp::cleanup()
{
	CI_LOG_V("shutdown");
	// save warp settings
	Warp::writeSettings(mWarps, writeFile(mWarpSettings));
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
	//Warp::setSize(mWarps, ivec2(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight));
	// tell the warps our window has been resized, so they properly scale up or down
	Warp::handleResize(mWarps);

}

void VideodrommControllerApp::mouseMove(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseMove(mWarps, event)) {
		// let your application perform its mouseMove handling here
	}
}

void VideodrommControllerApp::mouseDown(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseDown(mWarps, event)) {
		// let your application perform its mouseDown handling here
	}
}

void VideodrommControllerApp::mouseDrag(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseDrag(mWarps, event)) {
		// let your application perform its mouseDrag handling here
	}
}

void VideodrommControllerApp::mouseUp(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseUp(mWarps, event)) {
		// let your application perform its mouseUp handling here
	}
}

void VideodrommControllerApp::keyDown(KeyEvent event)
{
#if defined( CINDER_COCOA )
	bool isModDown = event.isMetaDown();
#else // windows
	bool isModDown = event.isControlDown();
#endif

	if (isModDown) {
		switch (event.getCode()) {
		case KeyEvent::KEY_s:
			fileWarpsName = "warps" + toString(getElapsedFrames()) + ".xml";
			mWarpSettings = getAssetPath("") / mVDSettings->mAssetsPath / fileWarpsName;
			Warp::writeSettings(mWarps, writeFile(mWarpSettings));
			mWarpSettings = getAssetPath("") / mVDSettings->mAssetsPath / "warps.xml";
			break;
		}
	}
	else {
		//fs::path moviePath;

		// pass this key event to the warp editor first
		if (!Warp::handleKeyDown(mWarps, event)) {
			// warp editor did not handle the key, so handle it here
			if (!mVDAnimation->handleKeyDown(event)) {
				// Animation did not handle the key, so handle it here

				switch (event.getCode()) {
				case KeyEvent::KEY_ESCAPE:
					// quit the application
					quit();
					break;
				case KeyEvent::KEY_w:
					// toggle warp edit mode
					Warp::enableEditMode(!Warp::isEditModeEnabled());
					break;
				case KeyEvent::KEY_LEFT:
					//mVDTextures->rewindMovie();				
					break;
				case KeyEvent::KEY_RIGHT:
					//mVDTextures->fastforwardMovie();				
					break;
				case KeyEvent::KEY_SPACE:
					//mVDTextures->playMovie();
					//mVDAnimation->currentScene++;
					//if (mMovie) { if (mMovie->isPlaying()) mMovie->stop(); else mMovie->play(); }
					break;
				case KeyEvent::KEY_0:
					mWarpFboIndex = 0;
					break;
				case KeyEvent::KEY_1:
					mWarpFboIndex = 1;
					break;
				case KeyEvent::KEY_2:
					mWarpFboIndex = 2;
					break;
				case KeyEvent::KEY_l:
					mVDAnimation->load();
					//mLoopVideo = !mLoopVideo;
					//if (mMovie) mMovie->setLoop(mLoopVideo);
					break;
				case KeyEvent::KEY_h:
					// mouse cursor
					mVDSettings->mCursorVisible = !mVDSettings->mCursorVisible;
					setUIVisibility(mVDSettings->mCursorVisible);

					break;
				case KeyEvent::KEY_n:
					mWarps.push_back(WarpPerspectiveBilinear::create());
					Warp::handleResize(mWarps);
					break;
				}
			}
		}
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
void VideodrommControllerApp::keyUp(KeyEvent event)
{
	// pass this key event to the warp editor first
	if (!Warp::handleKeyUp(mWarps, event)) {
		// let your application perform its keyUp handling here
		if (!mVDAnimation->handleKeyUp(event)) {
			// Animation did not handle the key, so handle it here
		}
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
	updateWindowTitle();
}
void VideodrommControllerApp::fileDrop(FileDropEvent event)
{
	int index = (int)(event.getX() / (mVDSettings->uiElementWidth + mVDSettings->uiMargin));// +1;
	ci::fs::path mPath = event.getFile(event.getNumFiles() - 1);
	string mFile = mPath.string();
	if (mMixes[0]->loadFileFromAbsolutePath(mFile, index) > -1) {
		// load success, reset zoom
		mVDAnimation->controlValues[22] = 1.0f;
	}
}

void VideodrommControllerApp::drawRenderWindow()
{
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

	// iterate over the warps and draw their content
	for (auto &warp : mWarps) {
		warp->draw(mMixes[0]->getTexture(mWarpFboIndex), Area(0, 0, mMixes[0]->getFboTextureWidth(mWarpFboIndex), mMixes[0]->getFboTextureHeight(mWarpFboIndex)));
	}
}

void VideodrommControllerApp::drawControlWindow()
{

	gl::clear(Color::black());
	//gl::color(Color::white());
	gl::setMatricesWindow(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, true);
	gl::draw(mMixes[0]->getTexture(), Rectf(10 + mVDSettings->uiLargeW, 170, 650 + mVDSettings->uiLargeW, 650));
	// imgui
	if (mVDUI->isReady()) {
#pragma region library
		/*mVDSettings->mRenderThumbs = true;

		xPos = margin;
		yPos = yPosRow2;
		for (int i = 0; i < mVDShaders->getCount(); i++)
		{
		//if (filter.PassFilter(mVDShaders->getShader(i).name.c_str()) && mVDShaders->getShader(i).active)
		if (mVDShaders->getShader(i).active)
		{
		TODO if (mVDSettings->iTrack == i) {
		sprintf(buf, "SEL ##lsh%d", i);
		}
		else {
		sprintf(buf, "%d##lsh%d", mVDShaders->getShader(i).microseconds, i);
		}
		sprintf(buf, "%s##lsh%d", mVDShaders->getShader(i).name.c_str(), i);
		ui::SetNextWindowSize(ImVec2(w, h));
		ui::SetNextWindowPos(ImVec2(xPos + margin, yPos));
		ui::Begin(buf, NULL, ImVec2(0, 0), ui::GetStyle().Alpha, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
		{
		xPos += w + inBetween;
		if (xPos > mVDSettings->MAX * w * 1.0)
		{
		xPos = margin;
		yPos += h + margin;
		}
		ui::PushID(i);
		ui::Image((void*)mVDTextures->getFboTextureId(i), ivec2(mVDSettings->mPreviewFboWidth, mVDSettings->mPreviewFboHeight));
		if (ui::IsItemHovered()) ui::SetTooltip(mVDShaders->getShader(i).name.c_str());

		//ui::Columns(2, "lr", false);
		// left
		if (mVDSettings->mLeftFragIndex == i)
		{
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.0f, 1.0f, 0.5f));
		}
		else
		{
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.0f, 0.1f, 0.1f));

		}
		ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(0.0f, 0.7f, 0.7f));
		ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(0.0f, 0.8f, 0.8f));
		sprintf(buf, "L##s%d", i);
		if (ui::Button(buf)) {
		mVDSettings->mLeftFragIndex = i;// TODO send via OSC? selectShader(true, i);
		mVDTextures->setFragForFbo(i, 1); // 2 = right;
		}
		if (ui::IsItemHovered()) ui::SetTooltip("Set shader to left");
		ui::PopStyleColor(3);
		//ui::NextColumn();
		ui::SameLine();
		// right
		if (mVDSettings->mRightFragIndex == i)
		{
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.3f, 1.0f, 0.5f));
		}
		else
		{
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.0f, 0.1f, 0.1f));
		}
		ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(0.3f, 0.7f, 0.7f));
		ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(0.3f, 0.8f, 0.8f));
		sprintf(buf, "R##s%d", i);
		if (ui::Button(buf)) {
		mVDSettings->mRightFragIndex = i;// TODO send via OSC? selectShader(false, i);
		mVDTextures->setFragForFbo(i, 2); // 2 = right;
		}
		if (ui::IsItemHovered()) ui::SetTooltip("Set shader to right");
		ui::PopStyleColor(3);
		//ui::NextColumn();
		ui::SameLine();
		// preview
		if (mVDSettings->mPreviewFragIndex == i)
		{
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.6f, 1.0f, 0.5f));
		}
		else
		{
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.0f, 0.1f, 0.1f));
		}
		ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(0.6f, 0.7f, 0.7f));
		ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(0.6f, 0.8f, 0.8f));
		sprintf(buf, "P##s%d", i);
		if (ui::Button(buf)) mVDSettings->mPreviewFragIndex = i;
		if (ui::IsItemHovered()) ui::SetTooltip("Preview shader");
		ui::PopStyleColor(3);

		// warp1
		if (mVDSettings->mWarp1FragIndex == i)
		{
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.16f, 1.0f, 0.5f));
		}
		else
		{
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.0f, 0.1f, 0.1f));
		}
		ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(0.16f, 0.7f, 0.7f));
		ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(0.16f, 0.8f, 0.8f));
		sprintf(buf, "1##s%d", i);
		if (ui::Button(buf)) mVDSettings->mWarp1FragIndex = i;
		if (ui::IsItemHovered()) ui::SetTooltip("Set warp 1 shader");
		ui::PopStyleColor(3);
		ui::SameLine();

		// warp2
		if (mVDSettings->mWarp2FragIndex == i)
		{
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.77f, 1.0f, 0.5f));
		}
		else
		{
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.0f, 0.1f, 0.1f));
		}
		ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(0.77f, 0.7f, 0.7f));
		ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(0.77f, 0.8f, 0.8f));
		sprintf(buf, "2##s%d", i);
		if (ui::Button(buf)) mVDSettings->mWarp2FragIndex = i;
		if (ui::IsItemHovered()) ui::SetTooltip("Set warp 2 shader");
		ui::PopStyleColor(3);

		// enable removing shaders
		if (i > 4)
		{
		ui::SameLine();
		sprintf(buf, "X##s%d", i);
		if (ui::Button(buf)) mVDShaders->removePixelFragmentShaderAtIndex(i);
		if (ui::IsItemHovered()) ui::SetTooltip("Remove shader");
		}

		ui::PopID();

		}
		ui::End();
		} // if filtered

		} // for
		*/
#pragma endregion library

#pragma region warps
		/*

		if (mVDSettings->mMode == MODE_WARP)
		{
		for (int i = 0; i < mBatchass->getWarpsRef()->getWarpsCount(); i++)
		{
		sprintf(buf, "Warp %d", i);
		ui::SetNextWindowSize(ImVec2(w, h));
		ui::Begin(buf);
		{
		ui::SetWindowPos(ImVec2((i * (w + inBetween)) + margin, yPos));
		ui::PushID(i);
		ui::Image((void*)mBatchass->getTexturesRef()->getFboTextureId(mVDSettings->mWarpFbos[i].textureIndex), ivec2(mVDSettings->mPreviewFboWidth, mVDSettings->mPreviewFboHeight));
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(i / 7.0f, 0.6f, 0.6f));
		ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(i / 7.0f, 0.7f, 0.7f));
		ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(i / 7.0f, 0.8f, 0.8f));
		sprintf(buf, "%d", mVDSettings->mWarpFbos[i].textureIndex);
		if (ui::SliderInt(buf, &mVDSettings->mWarpFbos[i].textureIndex, 0, mVDSettings->MAX - 1)) {
		}
		sprintf(buf, "%s", warpInputs[mVDSettings->mWarpFbos[i].textureIndex]);
		ui::Text(buf);

		ui::PopStyleColor(3);
		ui::PopID();
		}
		ui::End();
		}
		yPos += h + margin;
		}

		*/
#pragma endregion warps
	}
	mVDUI->Run("UI", (int)getAverageFps());
}

void VideodrommControllerApp::updateWindowTitle()
{
	getWindow()->setTitle("(" + mVDSettings->sFps + " fps) " + toString(mVDSettings->iBeat) + " Videodromm");
}

CINDER_APP(VideodrommControllerApp, RendererGl, &VideodrommControllerApp::prepare)
