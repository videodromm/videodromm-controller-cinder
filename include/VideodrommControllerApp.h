#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

// UserInterface
#include "CinderImGui.h"
// Settings
#include "VDSettings.h"
// Session
#include "VDSession.h"
// UI
#include "VDUI.h"

using namespace ci;
using namespace ci::app;
using namespace videodromm;

#define IM_ARRAYSIZE(_ARR)			((int)(sizeof(_ARR)/sizeof(*_ARR)))

class VideodrommControllerApp : public App
{
public:
	static void prepare(Settings *settings);

	void setup() override;
	void cleanup() override;
	void update() override;
	void drawRenderWindow();
	void drawControlWindow();

	void fileDrop(FileDropEvent event) override;
	//void resize() override;
	void resizeWindow();

	void mouseMove(MouseEvent event) override;
	void mouseDown(MouseEvent event) override;
	void mouseDrag(MouseEvent event) override;
	void mouseUp(MouseEvent event) override;

	void keyDown(KeyEvent event) override;
	void keyUp(KeyEvent event) override;

	void setUIVisibility(bool visible);
private:
	// Settings
	VDSettingsRef				mVDSettings;
	// Session
	VDSessionRef				mVDSession;
	// UI
	VDUIRef						mVDUI;

	// window mgmt
	WindowRef					mControlWindow;
	void						createControlWindow();
	void						deleteControlWindows();
	vector<WindowRef>			allWindows;
	void						positionRenderWindow();

	// movie
	bool						mMovieDelay;
	bool						mFadeInDelay;

};
