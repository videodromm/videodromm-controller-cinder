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
	mVDUI = VDUI::create(mVDSettings, mMixes[0], mVDRouter, mVDAnimation, mVDSession);

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
	mWarpFboIndex = 1;
	//Warp::setSize(mWarps, mImage->getSize());
	mSaveThumbTimer = 0.0f;

	// imgui
	mouseGlobal = false;
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

	mVDRouter->update();
	// check if a shader has been received from websockets
	if (mVDSettings->mShaderToLoad != "") {
		mMixes[0]->loadFboFragmentShader(mVDSettings->mShaderToLoad, 1);
	}
	updateWindowTitle();

	//renderSceneToFbo();
}
void VideodrommControllerApp::fileDrop(FileDropEvent event)
{
	int index = (int)(event.getX() / (mVDSettings->uiMargin + mVDSettings->uiElementWidth));
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
	ImGuiStyle& style = ui::GetStyle();
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

		style.WindowPadding = ImVec2(3, 3);
		style.FramePadding = ImVec2(2, 2);
		style.ItemSpacing = ImVec2(3, 3);
		style.ItemInnerSpacing = ImVec2(3, 3);
		style.WindowMinSize = ImVec2(mVDSettings->mPreviewFboWidth, mVDSettings->mPreviewFboHeight);
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

#pragma region menu
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New")) {}
			if (ImGui::MenuItem("Open", "Ctrl+O")) {}
			if (ImGui::BeginMenu("Open Recent"))
			{
				ImGui::MenuItem("live.frag");
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				// save warp settings
				Warp::writeSettings(mWarps, writeFile("warps1.xml"));
				// save params
				mVDSettings->save();
			}
			if (ImGui::MenuItem("Debug")) {}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Options"))
		{
			ImGui::DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f, "%.2f");
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("Quit", "Alt+F4")) { cleanup(); }
		ImGui::EndMainMenuBar();
	}
#pragma endregion menu

	showVDUI((int)getAverageFps());

#pragma region channels


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
		
		// Mouse
#pragma region mouse
		ui::SetNextWindowSize(ImVec2(mVDSettings->uiLargeW, mVDSettings->uiLargePreviewH), ImGuiSetCond_Once);
		ui::SetNextWindowPos(ImVec2(mVDSettings->uiXPosCol1, mVDSettings->uiMargin), ImGuiSetCond_Once);

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
void VideodrommControllerApp::showVDUI(unsigned int fps) {
	mVDUI->Run("UI", fps);
}
// If you're deploying to iOS, set the Render antialiasing to 0 for a significant
// performance improvement. This value defaults to 4 (AA_MSAA_4) on iOS and 16 (AA_MSAA_16)
// on the Desktop.
#if defined( CINDER_COCOA_TOUCH )
CINDER_APP(VideodrommControllerApp, RendererGl(RendererGl::AA_NONE))
#else
CINDER_APP(VideodrommControllerApp, RendererGl(RendererGl::Options().msaa(8)), &VideodrommControllerApp::prepare)
#endif
