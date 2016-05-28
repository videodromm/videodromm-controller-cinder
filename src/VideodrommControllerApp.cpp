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
	mVDSettings->mRenderThumbs = false;
	// Session
	mVDSession = VDSession::create(mVDSettings);
	// Utils
	mVDUtils = VDUtils::create(mVDSettings);
	// Animation
	mVDAnimation = VDAnimation::create(mVDSettings, mVDSession);
	// Message router
	mVDRouter = VDRouter::create(mVDSettings, mVDAnimation, mVDSession);
	// Image sequence
	CI_LOG_V("Assets folder: " + mVDUtils->getPath("").string());
	string imgSeqPath = mVDSession->getImageSequencePath();

	// Mix
	mMixesFilepath = getAssetPath("") / "mixes.xml";
	if (fs::exists(mMixesFilepath)) {
		// load textures from file if one exists
		mMixes = VDMix::readSettings(mVDSettings, mVDAnimation, loadFile(mMixesFilepath));
	}
	else {
		// otherwise create a texture from scratch
		mMixes.push_back(VDMix::create(mVDSettings, mVDAnimation));
	}

	setFrameRate(mVDSession->getTargetFps());
	mMovieDelay = mFadeInDelay = true;
	mIsResizing = true;
	mVDAnimation->tapTempo();
	mVDUtils->getWindowsResolution();
	// UI
	mVDUI = VDUI::create(mVDSettings, mMixes[0]);

	getWindow()->getSignalResize().connect(std::bind(&VideodrommControllerApp::resizeWindow, this));
	getWindow()->getSignalDraw().connect(std::bind(&VideodrommControllerApp::drawRenderWindow, this));
	// render fbo
	gl::Fbo::Format format;
	//format.setSamples( 4 ); // uncomment this to enable 4x antialiasing
	//mRenderFbo = gl::Fbo::create(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, format.colorTexture());
	if (mVDSettings->mStandalone) {

	}
	else {

		// OK mControlWindow = createWindow(Window::Format().size(mVDSettings->mMainWindowWidth, mVDSettings->mMainWindowHeight));
		mControlWindow = createWindow(Window::Format().size(1280, 700));
		mControlWindow->setPos(mVDSettings->mMainWindowX, mVDSettings->mMainWindowY);
		mControlWindow->setBorderless();
		mControlWindow->getSignalDraw().connect(std::bind(&VideodrommControllerApp::drawControlWindow, this));
		mControlWindow->getSignalResize().connect(std::bind(&VideodrommControllerApp::resizeWindow, this));

		// UI fbo
		//mUIFbo = gl::Fbo::create(mVDSettings->mMainWindowWidth, mVDSettings->mMainWindowHeight, format.colorTexture());
	}

	// warping
	gl::enableDepthRead();
	gl::enableDepthWrite();
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

	//mSrcArea = Area(0, 0, 700, 500);
	// TODO Warp::setSize(mWarps, mVDFbos[0]->getSize());
	// load image
	try {
		mImage = gl::Texture::create(loadImage(loadAsset("help.jpg")),
			gl::Texture2d::Format().loadTopDown().mipmap(true).minFilter(GL_LINEAR_MIPMAP_LINEAR));

		//mSrcArea = mImage->getBounds();

		// adjust the content size of the warps
		//Warp::setSize(mWarps, mImage->getSize());
	}
	catch (const std::exception &e) {
		console() << e.what() << std::endl;
	}
	//Warp::setSize(mWarps, ivec2(mVDSettings->mFboWidth, mVDSettings->mFboHeight));
	Warp::setSize(mWarps, ivec2(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight));
	mWarpFboIndex = 0;
	//Warp::setSize(mWarps, mImage->getSize());
	mSaveThumbTimer = 0.0f;

	// imgui
	margin = 3;
	inBetween = 3;
	// mPreviewFboWidth 80 mPreviewFboHeight 60 margin 10 inBetween 15 mPreviewWidth = 160;mPreviewHeight = 120;
	w = mVDSettings->mPreviewFboWidth + margin;
	h = mVDSettings->mPreviewFboHeight * 2.3;
	largeW = (mVDSettings->mPreviewFboWidth + margin) * 4;
	largeH = (mVDSettings->mPreviewFboHeight + margin) * 5;
	largePreviewW = mVDSettings->mPreviewWidth + margin;
	largePreviewH = (mVDSettings->mPreviewHeight + margin) * 2.4;
	displayHeight = mVDSettings->mMainWindowHeight - 50;
	yPosRow1 = largePreviewH + margin;
	yPosRow2 = yPosRow1 + largePreviewH + margin;
	yPosRow3 = yPosRow2 + h*1.4 + margin;
	xPosCol1 = mVDSettings->mFboWidth + margin;
	xPosCol2 = xPosCol1 + margin + largeW;
	xPosCol3 = xPosCol2 + margin;

	mouseGlobal = false;
	static float f = 0.0f;
	// mouse cursor
	if (mVDSettings->mCursorVisible)
	{
		showCursor();
	}
	else
	{
		hideCursor();
	}

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
	mIsResizing = true;
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
			case KeyEvent::KEY_l:
				mVDAnimation->load();
				//mLoopVideo = !mLoopVideo;
				//if (mMovie) mMovie->setLoop(mLoopVideo);
				break;
			case KeyEvent::KEY_c:
				// mouse cursor
				mVDSettings->mCursorVisible = !mVDSettings->mCursorVisible;
				if (mVDSettings->mCursorVisible) {
					hideCursor();
				}
				else {
					showCursor();
				}
				break;
			case KeyEvent::KEY_n:
				mWarps.push_back(WarpPerspectiveBilinear::create());
				break;
			case KeyEvent::KEY_a:
				fileWarpsName = "warps" + toString(getElapsedFrames()) + ".xml";
				mWarpSettings = getAssetPath("") / mVDSettings->mAssetsPath / fileWarpsName;
				Warp::writeSettings(mWarps, writeFile(mWarpSettings));
				mWarpSettings = getAssetPath("") / mVDSettings->mAssetsPath / "warps.xml";
				break;
			}
		}
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
	mVDAnimation->update();

	//mVDTextures->update();
	//mVDAudio->update();
	mVDRouter->update();
	updateWindowTitle();

	//renderSceneToFbo();
}
void VideodrommControllerApp::fileDrop(FileDropEvent event)
{
	int index = (int)(event.getX() / (margin + w));
	string ext = "";
	// use the last of the dropped files
	boost::filesystem::path mPath = event.getFile(event.getNumFiles() - 1);
	string mFile = mPath.string();
	int dotIndex = mFile.find_last_of(".");
	int slashIndex = mFile.find_last_of("\\");

	if (dotIndex != std::string::npos && dotIndex > slashIndex) ext = mFile.substr(mFile.find_last_of(".") + 1);

	if (ext == "wav" || ext == "mp3")
	{
	}
	else if (ext == "png" || ext == "jpg")
	{
		if (index < 1) index = 1;
		if (index > 3) index = 3;
		mMixes[0]->loadImageFile(mFile, index, true);
	}
	else if (ext == "glsl")
	{
		if (index > mMixes[0]->getFboCount() - 1) index = mMixes[0]->getFboCount() - 1;
		int rtn = mMixes[0]->loadFboFragmentShader(mFile, index);
		if (rtn > -1) {
			// load success
			// reset zoom
			mVDAnimation->controlValues[22] = 1.0f;
		}
	}
	else if (ext == "xml")
	{
	}
	else if (ext == "mov")
	{
		//mVDTextures->loadMovie(index, mFile);
	}
	else if (ext == "txt")
	{
	}
	else if (ext == "")
	{
		// try loading image sequence from dir
		//mVDTextures->loadImageSequence(index, mFile);
	}

}

// Render the scene into the FBO
/*void VideodrommControllerApp::renderSceneToFbo()
{
	// this will restore the old framebuffer binding when we leave this function
	// on non-OpenGL ES platforms, you can just call mFbo->unbindFramebuffer() at the end of the function
	// but this will restore the "screen" FBO on OpenGL ES, and does the right thing on both platforms
	gl::ScopedFramebuffer fbScp(mRenderFbo);
	gl::clear(Color::black());
	// setup the viewport to match the dimensions of the FBO
	gl::ScopedViewport scpVp(ivec2(0), mRenderFbo->getSize());
	
	gl::draw(mVDTextures->getFboTexture(2), Rectf(0, mVDSettings->mRenderHeight, mVDSettings->mRenderWidth, 0));

	int i = 0;
	// iterate over the warps and draw their content
	for (auto &warp : mWarps) {
		//warp->draw(mVDTextures->getFboTexture(i), mVDTextures->getFboTexture(i)->getBounds());
		//warp->draw(mVDTextures->getFboTexture(i), Area(0, 0, mVDTextures->getFboTextureWidth(i), mVDTextures->getFboTextureHeight(i)));
		warp->draw(mVDTextures->getFboTexture(i), Area(0, 0, mVDTextures->getFboTextureWidth(i), mVDTextures->getFboTextureHeight(i)));

		//warp->draw(mVDTextures->getFboTexture(i), Area(0, 0, 1280, 720));
		i++;
	}
}*/
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
	//gl::draw(mRenderFbo->getColorTexture());
	//gl::draw(mVDTextures->getFboTexture(2), Rectf(0, mVDSettings->mRenderHeight, mVDSettings->mRenderWidth, 0));

	//int i = 0;
	// iterate over the warps and draw their content
	for (auto &warp : mWarps) {
		warp->draw(mMixes[0]->getFboTexture(mWarpFboIndex), Area(0, 0, mMixes[0]->getFboTextureWidth(mWarpFboIndex), mMixes[0]->getFboTextureHeight(mWarpFboIndex)));

		//i++;
	}
}
// Render the UI into the FBO
//void VideodrommControllerApp::renderUIToFbo()
//{
	// this will restore the old framebuffer binding when we leave this function
	// on non-OpenGL ES platforms, you can just call mFbo->unbindFramebuffer() at the end of the function
	// but this will restore the "screen" FBO on OpenGL ES, and does the right thing on both platforms
	//gl::ScopedFramebuffer fbScp(mUIFbo);
	// setup the viewport to match the dimensions of the FBO
	//gl::ScopedViewport scpVp(ivec2(0), ivec2(mVDSettings->mFboWidth * mVDSettings->mUIZoom, mVDSettings->mFboHeight * mVDSettings->mUIZoom));

	//gl::draw(mUIFbo->getColorTexture());



//}
void VideodrommControllerApp::drawControlWindow()
{
	if (mIsResizing) {
		mIsResizing = false;
		if (mVDSettings->mStandalone) {
			// set ui window and io events callbacks 
			ui::connectWindow(getWindow());
		}
		else {
			ui::connectWindow(mControlWindow);
		}
		ui::initialize();

#pragma region style
		// our theme variables
		ImGuiStyle& style = ui::GetStyle();

		style.WindowPadding = ImVec2(3, 3);
		style.FramePadding = ImVec2(2, 2);
		style.ItemSpacing = ImVec2(3, 3);
		style.ItemInnerSpacing = ImVec2(3, 3);
		style.WindowMinSize = ImVec2(w, mVDSettings->mPreviewFboHeight);
		// new style
		style.Alpha = 1.0;
		style.WindowFillAlphaDefault = 0.83;
		style.ChildWindowRounding = 3;
		style.WindowRounding = 3;
		style.FrameRounding = 3;

		style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.00f, 0.40f, 0.41f, 1.00f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 1.00f, 1.00f, 0.65f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.44f, 0.80f, 0.80f, 0.18f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.44f, 0.80f, 0.80f, 0.27f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.44f, 0.81f, 0.86f, 0.66f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.18f, 0.21f, 0.73f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.27f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.22f, 0.29f, 0.30f, 0.71f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.44f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_ComboBg] = ImVec4(0.16f, 0.24f, 0.22f, 0.60f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 1.00f, 1.00f, 0.68f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.36f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.76f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.65f, 0.65f, 0.46f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.01f, 1.00f, 1.00f, 0.43f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.62f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.00f, 1.00f, 1.00f, 0.33f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.42f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);
		style.Colors[ImGuiCol_Column] = ImVec4(0.00f, 0.50f, 0.50f, 0.33f);
		style.Colors[ImGuiCol_ColumnHovered] = ImVec4(0.00f, 0.50f, 0.50f, 0.47f);
		style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_CloseButton] = ImVec4(0.00f, 0.78f, 0.78f, 0.35f);
		style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.00f, 0.78f, 0.78f, 0.47f);
		style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.00f, 0.78f, 0.78f, 1.00f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 1.00f, 1.00f, 0.22f);
		style.Colors[ImGuiCol_TooltipBg] = ImVec4(0.00f, 0.13f, 0.13f, 0.90f);
		style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.04f, 0.10f, 0.09f, 0.51f);
#pragma endregion style
	}

	//renderUIToFbo();
	gl::clear(Color::black());
	//gl::draw(mUIFbo->getColorTexture());

	//gl::color(Color::white());
	gl::setMatricesWindow(mVDSettings->mMainWindowWidth, mVDSettings->mMainWindowHeight, false);
	// imgui
	static int currentWindowRow1 = 0;
	static int currentWindowRow2 = 1;
	static int currentWindowRow3 = 0;
	static int selectedLeftInputTexture = 2;
	static int selectedRightInputTexture = 1;

	xPos = margin;
	const char* warpInputs[] = { "mix", "left", "right", "warp1", "warp2", "preview", "abp", "live", "w8", "w9", "w10", "w11", "w12", "w13", "w14", "w15" };

#pragma region Info

	ui::SetNextWindowSize(ImVec2(mVDSettings->mFboWidth, largePreviewH), ImGuiSetCond_Once);
	ui::SetNextWindowPos(ImVec2(xPos, margin), ImGuiSetCond_Once);
	sprintf(buf, "Videodromm Fps %c %d (target %.2f)###fps", "|/-\\"[(int)(ImGui::GetTime() / 0.25f) & 3], (int)mVDSettings->iFps, mVDSession->getTargetFps());
	ui::Begin(buf);
	{
		ImGui::PushItemWidth(mVDSettings->mPreviewFboWidth);
		ImGui::RadioButton("Audio", &currentWindowRow1, 0); ImGui::SameLine();
		ImGui::RadioButton("Midi", &currentWindowRow1, 1); ImGui::SameLine();
		ImGui::RadioButton("Chn", &currentWindowRow1, 2); ui::SameLine();
		ImGui::RadioButton("Mouse", &currentWindowRow1, 3); ui::SameLine();
		ImGui::RadioButton("Effects", &currentWindowRow1, 4); ui::SameLine();
		ImGui::RadioButton("Color", &currentWindowRow1, 5); ui::SameLine();
		ImGui::RadioButton("Tempo", &currentWindowRow1, 6); ui::SameLine();
		ImGui::RadioButton("Blend", &currentWindowRow1, 7);

		ImGui::RadioButton("Textures", &currentWindowRow2, 0); ImGui::SameLine();
		ImGui::RadioButton("Fbos", &currentWindowRow2, 1); ImGui::SameLine();
		ImGui::RadioButton("Shaders", &currentWindowRow2, 2);

		// fps
		static ImVector<float> values; if (values.empty()) { values.resize(100); memset(&values.front(), 0, values.size()*sizeof(float)); }
		static int values_offset = 0;
		static float refresh_time = -1.0f;
		if (ui::GetTime() > refresh_time + 1.0f / 6.0f)
		{
			refresh_time = ui::GetTime();
			values[values_offset] = mVDSettings->iFps;
			values_offset = (values_offset + 1) % values.size();
		}
		if (mVDSettings->iFps < 12.0) ui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
		ui::PlotLines("FPS", &values.front(), (int)values.size(), values_offset, mVDSettings->sFps.c_str(), 0.0f, mVDSession->getTargetFps(), ImVec2(0, 30));
		if (mVDSettings->iFps < 12.0) ui::PopStyleColor();

		ui::PopItemWidth();
		//ui::SameLine();
		//ui::Text("Target fps: %d", mVDSession->getTargetFps());

		mVDSettings->iDebug ^= ui::Button("Debug");
		ui::SameLine();
		if (ui::Button("Save Params"))
		{
			// save warp settings
			Warp::writeSettings(mWarps, writeFile("warps1.xml"));
			// save params
			mVDSettings->save();
		}
		ui::Text("Msg: %s", mVDSettings->mMsg.c_str());

	}
	ui::End();

#pragma endregion Info
	switch (currentWindowRow1) {
	case 0:
		// Audio
#pragma region Audio
		ui::SetNextWindowSize(ImVec2(largeW, largePreviewH), ImGuiSetCond_Once);
		ui::SetNextWindowPos(ImVec2(xPosCol1, margin), ImGuiSetCond_Once);
		ui::Begin("Audio");
		{
			static ImVector<float> timeValues; if (timeValues.empty()) { timeValues.resize(40); memset(&timeValues.front(), 0, timeValues.size()*sizeof(float)); }
			static int timeValues_offset = 0;
			// audio maxVolume
			static float tRefresh_time = -1.0f;
			if (ui::GetTime() > tRefresh_time + 1.0f / 20.0f)
			{
				tRefresh_time = ui::GetTime();
				timeValues[timeValues_offset] = mVDAnimation->maxVolume;
				timeValues_offset = (timeValues_offset + 1) % timeValues.size();
			}

			ui::SliderFloat("mult x", &mVDAnimation->controlValues[13], 0.01f, 10.0f);

			//ImGui::PlotHistogram("Histogram", mVDTextures->getSmallSpectrum(), 7, 0, NULL, 0.0f, 255.0f, ImVec2(0, 30));

			if (mVDAnimation->maxVolume > 240.0) ui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
			ui::PlotLines("Volume", &timeValues.front(), (int)timeValues.size(), timeValues_offset, toString(mVDUtils->formatFloat(mVDAnimation->maxVolume)).c_str(), 0.0f, 255.0f, ImVec2(0, 30));
			if (mVDAnimation->maxVolume > 240.0) ui::PopStyleColor();
		}
		ui::End();

#pragma endregion Audio	
		break;
	case 1:
#if defined( CINDER_MSW )
		// Midi
#pragma region MIDI

		ui::SetNextWindowSize(ImVec2(largeW, largePreviewH), ImGuiSetCond_Once);
		ui::SetNextWindowPos(ImVec2(xPosCol1, margin), ImGuiSetCond_Once);
		ui::Begin("MIDI");
		{
			sprintf(buf, "Enable");
			if (ui::Button(buf)) mVDRouter->midiSetup();
			if (ui::CollapsingHeader("MidiIn", "20", true, true))
			{
				ui::Columns(2, "data", true);
				ui::Text("Name"); ui::NextColumn();
				ui::Text("Connect"); ui::NextColumn();
				ui::Separator();

				for (int i = 0; i < mVDRouter->getMidiInPortsCount(); i++)
				{
					ui::Text(mVDRouter->getMidiInPortName(i).c_str()); ui::NextColumn();

					if (mVDRouter->isMidiInConnected(i))
					{
						sprintf(buf, "Disconnect %d", i);
					}
					else
					{
						sprintf(buf, "Connect %d", i);
					}

					if (ui::Button(buf))
					{
						if (mVDRouter->isMidiInConnected(i))
						{
							mVDRouter->closeMidiInPort(i);
						}
						else
						{
							mVDRouter->openMidiInPort(i);
						}
					}
					ui::NextColumn();
					ui::Separator();
				}
				ui::Columns(1);
			}
		}
		ui::End();
#pragma endregion MIDI
#endif
		break;
	case 2:
		// Channels

#pragma region channels
		ui::SetNextWindowSize(ImVec2(largeW, largePreviewH), ImGuiSetCond_Once);
		ui::SetNextWindowPos(ImVec2(xPosCol1, margin), ImGuiSetCond_Once);

		/*ui::Begin("Channels");
		{
		ui::Columns(3);
		ui::SetColumnOffset(0, 4.0f);// int column_index, float offset)
		ui::SetColumnOffset(1, 20.0f);// int column_index, float offset)
		//ui::SetColumnOffset(2, 24.0f);// int column_index, float offset)
		ui::Text("Chn"); ui::NextColumn();
		ui::Text("Tex"); ui::NextColumn();
		ui::Text("Name"); ui::NextColumn();
		ui::Separator();
		for (int i = 0; i < mVDSettings->MAX - 1; i++)
		{
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(i / 7.0f, 0.6f, 0.6f));
		ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(i / 7.0f, 0.7f, 0.7f));
		ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(i / 7.0f, 0.8f, 0.8f));
		ui::Text("c%d", i);
		ui::NextColumn();
		sprintf(buf, "%d", i);
		if (ui::SliderInt(buf, &mVDSettings->iChannels[i], 0, mVDSettings->MAX - 1)) {
		}
		ui::NextColumn();
		ui::PopStyleColor(3);
		ui::Text("%s", mVDTextures->getTextureName(mVDSettings->iChannels[i]));
		ui::NextColumn();
		}
		ui::Columns(1);
		}
		ui::End();*/


#pragma endregion channels
		break;
	case 3:
		// Mouse
#pragma region mouse
		ui::SetNextWindowSize(ImVec2(largeW, largePreviewH), ImGuiSetCond_Once);
		ui::SetNextWindowPos(ImVec2(xPosCol1, margin), ImGuiSetCond_Once);

		ui::Begin("Mouse");
		{
			ui::Text("Position: %.1f,%.1f", ui::GetIO().MousePos.x, ui::GetIO().MousePos.y);
			ui::Text("Clic %d", ui::GetIO().MouseDown[0]);
			mouseGlobal ^= ui::Button("mouse gbl");
			if (mouseGlobal)
			{
				mVDSettings->mRenderPosXY.x = ui::GetIO().MousePos.x; ui::SameLine();
				mVDSettings->mRenderPosXY.y = ui::GetIO().MousePos.y;
				mVDSettings->iMouse.z = ui::GetIO().MouseDown[0];
			}
			else
			{

				mVDSettings->iMouse.z = ui::Button("mouse click");
			}
			ui::SliderFloat("MouseX", &mVDSettings->mRenderPosXY.x, 0, mVDSettings->mFboWidth);
			ui::SliderFloat("MouseY", &mVDSettings->mRenderPosXY.y, 0, mVDSettings->mFboHeight);
		}
		ui::End();


#pragma endregion mouse
		break;
	case 4:
		// Effects
#pragma region effects
		ui::SetNextWindowSize(ImVec2(largeW, largePreviewH), ImGuiSetCond_Once);
		ui::SetNextWindowPos(ImVec2(xPosCol1, margin), ImGuiSetCond_Once);

		ui::Begin("Effects");
		{
			int hue = 0;

			(mVDSettings->iRepeat) ? ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(hue / 7.0f, 1.0f, 0.5f)) : ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(1.0f, 0.1f, 0.1f));
			ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(hue / 7.0f, 0.7f, 0.7f));
			ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(hue / 7.0f, 0.8f, 0.8f));
			mVDSettings->iRepeat ^= ui::Button("repeat");
			ui::PopStyleColor(3);
			hue++;
			ui::SameLine();

			(mVDAnimation->controlValues[45]) ? ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(hue / 7.0f, 1.0f, 0.5f)) : ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(1.0f, 0.1f, 0.1f));
			ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(hue / 7.0f, 0.7f, 0.7f));
			ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(hue / 7.0f, 0.8f, 0.8f));
			if (ui::Button("glitch")) { mVDAnimation->controlValues[45] = !mVDAnimation->controlValues[45]; }
			ui::PopStyleColor(3);
			hue++;

			(mVDAnimation->controlValues[46]) ? ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(hue / 7.0f, 1.0f, 0.5f)) : ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(1.0f, 0.1f, 0.1f));
			ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(hue / 7.0f, 0.7f, 0.7f));
			ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(hue / 7.0f, 0.8f, 0.8f));
			if (ui::Button("toggle")) { mVDAnimation->controlValues[46] = !mVDAnimation->controlValues[46]; }
			ui::PopStyleColor(3);
			hue++;
			ui::SameLine();

			(mVDAnimation->controlValues[47]) ? ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(hue / 7.0f, 1.0f, 0.5f)) : ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(1.0f, 0.1f, 0.1f));
			ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(hue / 7.0f, 0.7f, 0.7f));
			ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(hue / 7.0f, 0.8f, 0.8f));
			if (ui::Button("vignette")) { mVDAnimation->controlValues[47] = !mVDAnimation->controlValues[47]; }
			ui::PopStyleColor(3);
			hue++;

			(mVDAnimation->controlValues[48]) ? ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(hue / 7.0f, 1.0f, 0.5f)) : ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(1.0f, 0.1f, 0.1f));
			ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(hue / 7.0f, 0.7f, 0.7f));
			ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(hue / 7.0f, 0.8f, 0.8f));
			if (ui::Button("invert")) { mVDAnimation->controlValues[48] = !mVDAnimation->controlValues[48]; }
			ui::PopStyleColor(3);
			hue++;
			ui::SameLine();

			(mVDSettings->iGreyScale) ? ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(hue / 7.0f, 1.0f, 0.5f)) : ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(1.0f, 0.1f, 0.1f));
			ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(hue / 7.0f, 0.7f, 0.7f));
			ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(hue / 7.0f, 0.8f, 0.8f));
			mVDSettings->iGreyScale ^= ui::Button("greyscale");
			ui::PopStyleColor(3);
			hue++;

			if (ui::Button("blackout"))
			{
				mVDAnimation->controlValues[1] = mVDAnimation->controlValues[2] = mVDAnimation->controlValues[3] = mVDAnimation->controlValues[4] = 0.0;
				mVDAnimation->controlValues[5] = mVDAnimation->controlValues[6] = mVDAnimation->controlValues[7] = mVDAnimation->controlValues[8] = 0.0;
			}
		}
		ui::End();


#pragma endregion effects
		break;
	case 5:
		// Color
#pragma region Color
		ui::SetNextWindowSize(ImVec2(largeW, largePreviewH), ImGuiSetCond_Once);
		ui::SetNextWindowPos(ImVec2(xPosCol1, margin), ImGuiSetCond_Once);

		ui::Begin("Color");
		{
			stringstream sParams;
			bool colorChanged = false;
			sParams << "{\"params\" :[{\"name\" : 0,\"value\" : " << getElapsedFrames() << "}"; // TimeStamp
			// foreground color
			color[0] = mVDAnimation->controlValues[1];
			color[1] = mVDAnimation->controlValues[2];
			color[2] = mVDAnimation->controlValues[3];
			color[3] = mVDAnimation->controlValues[4];
			ui::ColorEdit4("f", color);

			for (int i = 0; i < 4; i++)
			{
				if (mVDAnimation->controlValues[i + 1] != color[i])
				{
					sParams << ",{\"name\" : " << i + 1 << ",\"value\" : " << color[i] << "}";
					mVDAnimation->controlValues[i + 1] = color[i];
					colorChanged = true;
				}
			}
			if (colorChanged) mVDRouter->colorWrite(); //lights4events

			// background color
			backcolor[0] = mVDAnimation->controlValues[5];
			backcolor[1] = mVDAnimation->controlValues[6];
			backcolor[2] = mVDAnimation->controlValues[7];
			backcolor[3] = mVDAnimation->controlValues[8];
			ui::ColorEdit4("g", backcolor);
			for (int i = 0; i < 4; i++)
			{
				if (mVDAnimation->controlValues[i + 5] != backcolor[i])
				{
					sParams << ",{\"name\" : " << i + 5 << ",\"value\" : " << backcolor[i] << "}";
					mVDAnimation->controlValues[i + 5] = backcolor[i];
				}

			}
			// color multipliers
			if (ui::Button("x##RedX")) { mVDSettings->iRedMultiplier = 1.0f; }
			ui::SameLine();
			if (ui::SliderFloat("RedX", &mVDSettings->iRedMultiplier, 0.0f, 3.0f))
			{
			}
			if (ui::Button("x##GreenX")) { mVDSettings->iGreenMultiplier = 1.0f; }
			ui::SameLine();
			if (ui::SliderFloat("GreenX", &mVDSettings->iGreenMultiplier, 0.0f, 3.0f))
			{
			}
			if (ui::Button("x##BlueX")) { mVDSettings->iBlueMultiplier = 1.0f; }
			ui::SameLine();
			if (ui::SliderFloat("BlueX", &mVDSettings->iBlueMultiplier, 0.0f, 3.0f))
			{
			}

			sParams << "]}";
			string strParams = sParams.str();
			if (strParams.length() > 60)
			{
				mVDRouter->sendJSON(strParams);
			}
		}
		ui::End();


#pragma endregion Color
		break;
	case 6:
		// Tempo
#pragma region Tempo
		ui::SetNextWindowSize(ImVec2(largeW, largePreviewH), ImGuiSetCond_Once);
		ui::SetNextWindowPos(ImVec2(xPosCol1, margin), ImGuiSetCond_Once);

		ui::Begin("Tempo");
		{
			if (ui::Button("x##spdx")) { mVDSettings->iSpeedMultiplier = 1.0; }
			ui::SameLine();
			ui::SliderFloat("speed x", &mVDSettings->iSpeedMultiplier, 0.01f, 5.0f, "%.1f");

			ui::Text("Beat %d ", mVDSettings->iBeat);
			ui::SameLine();
			ui::Text("Beat Idx %d ", mVDAnimation->iBeatIndex);
			//ui::SameLine();
			//ui::Text("Bar %d ", mVDAnimation->iBar);


			if (ui::Button("x##bpbx")) { mVDSession->iBeatsPerBar = 1; }
			ui::SameLine();
			ui::SliderInt("beats/bar", &mVDSession->iBeatsPerBar, 1, 8);


			ui::Text("Time %.2f", mVDSettings->iGlobalTime);
			ui::SameLine();
			ui::Text("Trk %s %.2f", mVDSettings->mTrackName.c_str(), mVDSettings->liveMeter);
			//			ui::Checkbox("Playing", &mVDSettings->mIsPlaying);

			ui::Text("Tempo %.2f ", mVDSession->getBpm());


			if (ui::Button("Tap tempo")) { mVDAnimation->tapTempo(); }

			if (ui::Button("Time tempo")) { mVDAnimation->mUseTimeWithTempo = !mVDAnimation->mUseTimeWithTempo; }


			//void Batchass::setTimeFactor(const int &aTimeFactor)
			ui::SliderFloat("time x", &mVDAnimation->iTimeFactor, 0.0001f, 1.0f, "%.01f");
			ui::SameLine();

		}
		ui::End();


#pragma endregion Tempo
		break;
	case 7:
		// Blend
#pragma region Blend
		ui::SetNextWindowSize(ImVec2(largeW, largePreviewH), ImGuiSetCond_Once);
		ui::SetNextWindowPos(ImVec2(xPosCol1, margin), ImGuiSetCond_Once);

		ui::Begin("Blend");
		{
			// blend modes
			if (ui::Button("x##blendmode")) { mVDSettings->iBlendMode = 0.0f; }
			ui::SameLine();
			ui::SliderInt("blendmode", &mVDSettings->iBlendMode, 0, mVDAnimation->maxBlendMode);

		}
		ui::End();
#pragma endregion Blend
		break;
	}

#pragma region Global

	ui::SetNextWindowSize(ImVec2(largeW, displayHeight), ImGuiSetCond_Once);
	ui::SetNextWindowPos(ImVec2(xPosCol2, margin), ImGuiSetCond_Once);
	ui::Begin("Animation");
	{
		ImGui::PushItemWidth(mVDSettings->mPreviewFboWidth);


		if (ui::CollapsingHeader("Animation", NULL, true, true))
		{

			ui::SliderInt("UI Zoom", &mVDSettings->mUIZoom, 1, 8);
			int ctrl;
			stringstream aParams;
			aParams << "{\"params\" :[{\"name\" : 0,\"value\" : " << getElapsedFrames() << "}"; // TimeStamp

			// iChromatic
			ctrl = 10;
			if (ui::Button("a##chromatic")) { mVDAnimation->lockChromatic(); }
			ui::SameLine();
			if (ui::Button("t##chromatic")) { mVDAnimation->tempoChromatic(); }
			ui::SameLine();
			if (ui::Button("x##chromatic")) { mVDAnimation->resetChromatic(); }
			ui::SameLine();
			if (ui::SliderFloat("chromatic/min/max", &mVDAnimation->controlValues[ctrl], mVDAnimation->minChromatic, mVDAnimation->maxChromatic))
			{
				aParams << ",{\"name\" : " << ctrl << ",\"value\" : " << mVDAnimation->controlValues[ctrl] << "}";
			}

			// ratio
			ctrl = 11;
			if (ui::Button("a##ratio")) { mVDAnimation->lockRatio(); }
			ui::SameLine();
			if (ui::Button("t##ratio")) { mVDAnimation->tempoRatio(); }
			ui::SameLine();
			if (ui::Button("x##ratio")) { mVDAnimation->resetRatio(); }
			ui::SameLine();
			if (ui::SliderFloat("ratio/min/max", &mVDAnimation->controlValues[ctrl], mVDAnimation->minRatio, mVDAnimation->maxRatio))
			{
				aParams << ",{\"name\" : " << ctrl << ",\"value\" : " << mVDAnimation->controlValues[ctrl] << "}";
			}
			// exposure
			ctrl = 14;
			if (ui::Button("a##exposure")) { mVDAnimation->lockExposure(); }
			ui::SameLine();
			if (ui::Button("t##exposure")) { mVDAnimation->tempoExposure(); }
			ui::SameLine();
			if (ui::Button("x##exposure")) { mVDAnimation->resetExposure(); }
			ui::SameLine();
			if (ui::DragFloat("exposure", &mVDAnimation->controlValues[ctrl], 0.1f, mVDAnimation->minExposure, mVDAnimation->maxExposure))
			{
				aParams << ",{\"name\" : " << ctrl << ",\"value\" : " << mVDAnimation->controlValues[ctrl] << "}";
				mVDAnimation->setExposure(mVDAnimation->controlValues[ctrl]);
			}
			// zoom
			ctrl = 22;
			if (ui::Button("a##zoom"))
			{
				mVDAnimation->lockZoom();
			}
			ui::SameLine();
			if (ui::Button("t##zoom")) { mVDAnimation->tempoZoom(); }
			ui::SameLine();
			if (ui::Button("x##zoom")) { mVDAnimation->resetZoom(); }
			ui::SameLine();
			if (ui::DragFloat("zoom", &mVDAnimation->controlValues[ctrl], 0.1f, mVDAnimation->minZoom, mVDAnimation->maxZoom))
			{
				aParams << ",{\"name\" : " << ctrl << ",\"value\" : " << mVDAnimation->controlValues[ctrl] << "}";
			}
			// z position
			ctrl = 9;
			if (ui::Button("a##zpos")) { mVDAnimation->lockZPos(); }
			ui::SameLine();
			if (ui::Button("t##zpos")) { mVDAnimation->tempoZPos(); }
			ui::SameLine();
			if (ui::Button("x##zpos")) { mVDAnimation->resetZPos(); }
			ui::SameLine();
			if (ui::SliderFloat("zPosition", &mVDAnimation->controlValues[ctrl], mVDAnimation->minZPos, mVDAnimation->maxZPos))
			{
				aParams << ",{\"name\" : " << ctrl << ",\"value\" : " << mVDAnimation->controlValues[ctrl] << "}";
			}

			// rotation speed 
			ctrl = 19;
			if (ui::Button("a##rotationspeed")) { mVDAnimation->lockRotationSpeed(); }
			ui::SameLine();
			if (ui::Button("t##rotationspeed")) { mVDAnimation->tempoRotationSpeed(); }
			ui::SameLine();
			if (ui::Button("x##rotationspeed")) { mVDAnimation->resetRotationSpeed(); }
			ui::SameLine();
			if (ui::DragFloat("rotationSpeed", &mVDAnimation->controlValues[ctrl], 0.01f, mVDAnimation->minRotationSpeed, mVDAnimation->maxRotationSpeed))
			{
				aParams << ",{\"name\" : " << ctrl << ",\"value\" : " << mVDAnimation->controlValues[ctrl] << "}";
			}
			// badTv
			/*if (ui::Button("x##badtv")) { mVDSettings->iBadTv = 0.0f; }
			ui::SameLine();
			if (ui::SliderFloat("badTv/min/max", &mVDSettings->iBadTv, 0.0f, 5.0f))
			{
			}*/
			// param1
			if (ui::Button("x##param1")) { mVDSettings->iParam1 = 1.0f; }
			ui::SameLine();
			if (ui::SliderFloat("param1/min/max", &mVDSettings->iParam1, 0.01f, 100.0f))
			{
			}
			// param2
			if (ui::Button("x##param2")) { mVDSettings->iParam2 = 1.0f; }
			ui::SameLine();
			if (ui::SliderFloat("param2/min/max", &mVDSettings->iParam2, 0.01f, 100.0f))
			{
			}
			sprintf(buf, "XorY");
			mVDSettings->iXorY ^= ui::Button(buf);

			// steps
			ctrl = 20;
			if (ui::Button("x##steps")) { mVDAnimation->controlValues[ctrl] = 16.0f; }
			ui::SameLine();
			if (ui::SliderFloat("steps", &mVDAnimation->controlValues[ctrl], 1.0f, 128.0f))
			{
				aParams << ",{\"name\" : " << ctrl << ",\"value\" : " << mVDAnimation->controlValues[ctrl] << "}";
			}
			// pixelate
			ctrl = 15;
			if (ui::Button("x##pixelate")) { mVDAnimation->controlValues[ctrl] = 1.0f; }
			ui::SameLine();
			if (ui::SliderFloat("pixelate", &mVDAnimation->controlValues[ctrl], 0.01f, 1.0f))
			{
				aParams << ",{\"name\" : " << ctrl << ",\"value\" : " << mVDAnimation->controlValues[ctrl] << "}";
			}
			// trixels
			ctrl = 16;
			if (ui::Button("x##trixels")) { mVDAnimation->controlValues[ctrl] = 0.0f; }
			ui::SameLine();
			if (ui::SliderFloat("trixels", &mVDAnimation->controlValues[ctrl], 0.00f, 1.0f))
			{
				aParams << ",{\"name\" : " << ctrl << ",\"value\" : " << mVDAnimation->controlValues[ctrl] << "}";
			}
			// grid
			ctrl = 17;
			if (ui::Button("x##grid")) { mVDAnimation->controlValues[ctrl] = 0.0f; }
			ui::SameLine();
			if (ui::SliderFloat("grid", &mVDAnimation->controlValues[ctrl], 0.00f, 60.0f))
			{
				aParams << ",{\"name\" : " << ctrl << ",\"value\" : " << mVDAnimation->controlValues[ctrl] << "}";
			}

			aParams << "]}";
			string strAParams = aParams.str();
			if (strAParams.length() > 60)
			{
				mVDRouter->sendJSON(strAParams);

			}
		}
		ui::PopItemWidth();


		if (ui::CollapsingHeader("Camera", NULL, true, true))
		{
			ui::SliderFloat("Pos.x", &mVDSettings->mRenderPosXY.x, 0.0f, mVDSettings->mRenderWidth);
			ui::SliderFloat("Pos.y", &mVDSettings->mRenderPosXY.y, 0.0f, mVDSettings->mRenderHeight);
			float eyeZ = mVDSettings->mCamera.getEyePoint().z;
			if (ui::SliderFloat("Eye.z", &eyeZ, -500.0f, 1.0f))
			{
				vec3 eye = mVDSettings->mCamera.getEyePoint();
				eye.z = eyeZ;
				mVDSettings->mCamera.setEyePoint(eye);
			}
			ui::SliderFloat("ABP Bend", &mVDSettings->mBend, -20.0f, 20.0f);

		}

	}
	ui::End();

#pragma endregion Global
	xPos = margin;

#pragma region left
	// push color for this chain, must be popped at the end
	ui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.0f, 1.0f, 0.0f, 1.00f));

	ui::SetNextWindowSize(ImVec2(largePreviewW, largePreviewH), ImGuiSetCond_Once);
	ui::SetNextWindowPos(ImVec2(xPos, yPosRow1), ImGuiSetCond_Once);
	ui::Begin("Left input texture");
	{
		ui::PushItemWidth(mVDSettings->mPreviewFboWidth);
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.1f, 0.6f, 0.6f));
		ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(0.1f, 0.7f, 0.7f));
		ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(0.1f, 0.8f, 0.8f));

		ui::Image((void*)mMixes[0]->getLeftFboTexture()->getId(), ivec2(mVDSettings->mPreviewFboWidth, mVDSettings->mPreviewFboHeight));

		//ui::SliderInt("Input", &selectedLeftInputTexture, 0, mVDTextures->getInputTexturesCount() - 1);

		ui::PopStyleColor(3);
		ui::PopItemWidth();
	}
	ui::End();
	xPos += largePreviewW + margin;

	// pop color for this chain
	ui::PopStyleColor(1);
#pragma endregion left

#pragma region right
	// push color for this chain, must be popped at the end
	ui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.0f, 1.0f, 0.0f, 1.00f));

	ui::SetNextWindowSize(ImVec2(largePreviewW, largePreviewH), ImGuiSetCond_Once);
	ui::SetNextWindowPos(ImVec2(xPos, yPosRow1), ImGuiSetCond_Once);
	ui::Begin("Right input texture");
	{
		ui::PushItemWidth(mVDSettings->mPreviewFboWidth);
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.1f, 0.6f, 0.6f));
		ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(0.1f, 0.7f, 0.7f));
		ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(0.1f, 0.8f, 0.8f));

		ui::Image((void*)mMixes[0]->getRightFboTexture()->getId(), ivec2(mVDSettings->mPreviewFboWidth, mVDSettings->mPreviewFboHeight));

		//ui::SliderInt("Input", &selectedRightInputTexture, 0, mVDTextures->getInputTexturesCount() - 1);

		ui::PopStyleColor(3);
		ui::PopItemWidth();
	}
	ui::End();
	xPos += largePreviewW + margin;

	// pop color for this chain
	ui::PopStyleColor(1);
#pragma endregion right

#pragma region mix
	// left/warp1 fbo
	ui::SetNextWindowSize(ImVec2(largePreviewW, largePreviewH), ImGuiSetCond_Once);
	ui::SetNextWindowPos(ImVec2(xPos, yPosRow1), ImGuiSetCond_Once);
	ui::Begin("Left/warp1 fbo");
	{
		ui::PushItemWidth(mVDSettings->mPreviewFboWidth);
		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.1f, 0.6f, 0.6f));
		ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(0.1f, 0.7f, 0.7f));
		ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(0.1f, 0.8f, 0.8f));

		sprintf(buf, "FV##mix%d", 40);
		// weird ui::Image((void*)mVDTextures->getFboTextureId(mWarpFboIndex), ivec2(mVDSettings->mPreviewWidth, mVDSettings->mPreviewHeight));
		ui::Image((void*)mMixes[0]->getTexture()->getId(), ivec2(mVDSettings->mPreviewWidth, mVDSettings->mPreviewHeight));

		//ui::SliderInt("Fbo", &mWarpFboIndex, 0, mVDTextures->getFboCount() - 1);

		ui::PopStyleColor(3);
		ui::PopItemWidth();
	}
	ui::End();
	xPos += largePreviewW + margin;

	// right/warp2 fbo
	ui::SetNextWindowSize(ImVec2(largePreviewW, largePreviewH), ImGuiSetCond_Once);
	ui::SetNextWindowPos(ImVec2(xPos, yPosRow1), ImGuiSetCond_Once);
	ui::Begin("Right/warp2 fbo");
	{
		ui::PushItemWidth(mVDSettings->mPreviewFboWidth);

		ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.1f, 0.6f, 0.6f));
		ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(0.1f, 0.7f, 0.7f));
		ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(0.1f, 0.8f, 0.8f));

		sprintf(buf, "FV##f%d", 41);

		ui::Image((void*)mImage->getId(), ivec2(mVDSettings->mPreviewWidth, mVDSettings->mPreviewHeight));

		ui::PopStyleColor(3);
		ui::PopItemWidth();
	}
	ui::End();
	xPos += largePreviewW + margin;

#pragma endregion mix
	xPos = margin;
	showVDUI(currentWindowRow2);

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

	xPos = margin;
	switch (currentWindowRow3) {


	case 3:
		// Blendmodes

		break;

	}

	// next line
	xPos = margin;

	/*
	#pragma region warps
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
	#pragma endregion warps
	*/

}

void VideodrommControllerApp::updateWindowTitle()
{
	getWindow()->setTitle("(" + mVDSettings->sFps + " fps) " + toString(mVDSettings->iBeat) + " Videodromm");

}
// UI
void VideodrommControllerApp::showVDUI(unsigned int window) {
	mVDUI->Run("UI", window);
}
// If you're deploying to iOS, set the Render antialiasing to 0 for a significant
// performance improvement. This value defaults to 4 (AA_MSAA_4) on iOS and 16 (AA_MSAA_16)
// on the Desktop.
#if defined( CINDER_COCOA_TOUCH )
CINDER_APP(VideodrommControllerApp, RendererGl(RendererGl::AA_NONE))
#else
CINDER_APP(VideodrommControllerApp, RendererGl(RendererGl::Options().msaa(8)), &VideodrommControllerApp::prepare)
#endif
