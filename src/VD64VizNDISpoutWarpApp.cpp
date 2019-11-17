#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

// Settings
#include "VDSettings.h"
// Session
#include "VDSession.h"
// Log
#include "VDLog.h"
// Spout
#include "CiSpoutIn.h"
// NDI
#include "CinderNDIReceiver.h"
// Warping
#include "Warp.h"
// UI
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS 1
#include "VDUI.h"
#define IM_ARRAYSIZE(_ARR)			((int)(sizeof(_ARR)/sizeof(*_ARR)))

using namespace ci;
using namespace ci::app;
using namespace ph::warping;
using namespace std;
using namespace videodromm;

class VD64VizNDISpoutWarpApp : public App {

public:
	VD64VizNDISpoutWarpApp();
	void mouseMove(MouseEvent event) override;
	void mouseDown(MouseEvent event) override;
	void mouseDrag(MouseEvent event) override;
	void mouseUp(MouseEvent event) override;
	void keyDown(KeyEvent event) override;
	void keyUp(KeyEvent event) override;
	void fileDrop(FileDropEvent event) override;
	void update() override;
	void draw() override;
	void cleanup() override;
	void toggleUIVisibility() { mVDSession->toggleUI(); };
	void toggleCursorVisibility(bool visible);
private:
	// Settings
	VDSettingsRef					mVDSettings;
	// Session
	VDSessionRef					mVDSession;
	// Log
	VDLogRef						mVDLog;
	// UI
	VDUIRef							mVDUI;
	// handle resizing for imgui
	void							resizeWindow();

	string							mError;
	bool							mIsShutDown;
	Anim<float>						mRenderWindowTimer;
	void							positionRenderWindow();
	bool							mFadeInDelay;
	// Spout
	SpoutIn							mSpoutIn;
	// NDI
	CinderNDIReceiver				mReceiver;
	string							first = "";
	long							second = 0;
	// warp
	WarpList						mWarps;
	fs::path						mSettings;
};


VD64VizNDISpoutWarpApp::VD64VizNDISpoutWarpApp()
	: mReceiver{}
{
	// Settings
	mVDSettings = VDSettings::create("VD64VizNDISpoutWarp");
	// Session
	mVDSession = VDSession::create(mVDSettings);
	//mVDSettings->mCursorVisible = true;
	toggleCursorVisibility(mVDSettings->mCursorVisible);
	mVDSession->getWindowsResolution();

	mFadeInDelay = true;
	// UI
	mVDUI = VDUI::create(mVDSettings, mVDSession);
	// initialize warps
	mSettings = getAssetPath("") / "warps.xml";
	if (fs::exists(mSettings)) {
		// load warp settings from file if one exists
		mWarps = Warp::readSettings(loadFile(mSettings));
	}
	else {
		// otherwise create a warp from scratch
		mWarps.push_back(WarpPerspectiveBilinear::create());
	}
	// NDI
	mReceiver.setup("receiver-vdviz");
	// windows
	mIsShutDown = false;
	mRenderWindowTimer = 0.0f;
	timeline().apply(&mRenderWindowTimer, 1.0f, 2.0f).finishFn([&] { positionRenderWindow(); });

}
void VD64VizNDISpoutWarpApp::resizeWindow()
{
	// tell the warps our window has been resized, so they properly scale up or down
	Warp::handleResize(mWarps);
	mVDUI->resize();
}
void VD64VizNDISpoutWarpApp::positionRenderWindow() {
	mVDSession->getWindowsResolution();
	mVDSettings->mRenderPosXY = ivec2(mVDSettings->mRenderX, mVDSettings->mRenderY);
	setWindowPos(mVDSettings->mRenderX, mVDSettings->mRenderY);
	setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);
}
void VD64VizNDISpoutWarpApp::toggleCursorVisibility(bool visible)
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
void VD64VizNDISpoutWarpApp::fileDrop(FileDropEvent event)
{
	mVDSession->fileDrop(event);
}
void VD64VizNDISpoutWarpApp::update()
{
	mVDSession->setFloatUniformValueByIndex(mVDSettings->IFPS, getAverageFps());
	mVDSession->update();
	mReceiver.update();
	if (mReceiver.isReady()) {
		int senderIndex = mReceiver.getCurrentSenderIndex();
		int senderCount = mReceiver.getNumberOfSendersFound();
		std::string senderName = mReceiver.getCurrentSenderName();
		getWindow()->setTitle("#" + std::to_string(senderIndex) + "/" + std::to_string(senderCount) + ": " + senderName + " @ " + std::to_string((int)getAverageFps()) + " fps " + first + " - " + std::to_string(second));
	}
	else {
		getWindow()->setTitle(std::to_string((int)getAverageFps()) + " fps connecting...");
	}
	//getWindow()->setTitle(std::to_string((int)getAverageFps()) + " fps " + first + " - " + std::to_string(second) + " - NDI-Receiver Spout-Sender");

}
void VD64VizNDISpoutWarpApp::cleanup()
{
	if (!mIsShutDown)
	{
		mIsShutDown = true;
		CI_LOG_V("shutdown");
		// save warp settings
		Warp::writeSettings(mWarps, writeFile(mSettings));
		// save settings
		mVDSettings->save();
		mVDSession->save();
		quit();
	}
}
void VD64VizNDISpoutWarpApp::mouseMove(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseMove(mWarps, event)) {
		if (!mVDSession->handleMouseMove(event)) {
			// let your application perform its mouseMove handling here
		}
	}
}
void VD64VizNDISpoutWarpApp::mouseDown(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseDown(mWarps, event)) {
		// let your application perform its mouseDown handling here
		if (event.isRightDown()) {
			// Select a sender
			// SpoutPanel.exe must be in the executable path
			mSpoutIn.getSpoutReceiver().SelectSenderPanel(); // DirectX 11 by default
		}
		if (!mVDSession->handleMouseDown(event)) {

		}
	}
}
void VD64VizNDISpoutWarpApp::mouseDrag(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseDrag(mWarps, event)) {

		if (!mVDSession->handleMouseDrag(event)) {
			// let your application perform its mouseDrag handling here
		}
	}
}
void VD64VizNDISpoutWarpApp::mouseUp(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseUp(mWarps, event)) {
		if (!mVDSession->handleMouseUp(event)) {
			// let your application perform its mouseUp handling here
		}
	}
}

void VD64VizNDISpoutWarpApp::keyDown(KeyEvent event)
{
	/*
	if (event.getCode() == KeyEvent::KEY_UP) {
		int currentIndex = mReceiver.getCurrentSenderIndex();
		if (currentIndex < mReceiver.getNumberOfSendersFound() -1) {
			mReceiver.switchSource(currentIndex + 1);
		}
	}
	if (event.getCode() == KeyEvent::KEY_DOWN) {
		int currentIndex = mReceiver.getCurrentSenderIndex();
		if (currentIndex >= 1) {
			mReceiver.switchSource(currentIndex - 1);
		}
	}
	
	*/
	// pass this key event to the warp editor first
	if (!Warp::handleKeyDown(mWarps, event)) {

		if (!mVDSession->handleKeyDown(event)) {
			switch (event.getCode()) {
			case KeyEvent::KEY_ESCAPE:
				// quit the application
				quit();
				break;
			case KeyEvent::KEY_w:
				// toggle warp edit mode
				Warp::enableEditMode(!Warp::isEditModeEnabled());
				break;
			case KeyEvent::KEY_c:
				// mouse cursor and ui visibility
				mVDSettings->mCursorVisible = !mVDSettings->mCursorVisible;
				toggleCursorVisibility(mVDSettings->mCursorVisible);
				break;
			case KeyEvent::KEY_F11:
				// windows position
				positionRenderWindow();
				break;
			}
		}
	}
}
void VD64VizNDISpoutWarpApp::keyUp(KeyEvent event)
{
	// pass this key event to the warp editor first
	if (!Warp::handleKeyUp(mWarps, event)) {

		if (!mVDSession->handleKeyUp(event)) {
		}
	}
}

void VD64VizNDISpoutWarpApp::draw()
{
	gl::clear(Color::black());
	if (mFadeInDelay) {
		mVDSettings->iAlpha = 0.0f;
		if (getElapsedFrames() > mVDSession->getFadeInDelay()) {
			mFadeInDelay = false;
			timeline().apply(&mVDSettings->iAlpha, 0.0f, 1.0f, 1.5f, EaseInCubic());
		}
	}

	//gl::setMatricesWindow(toPixels(getWindowSize()),false);
	gl::setMatricesWindow(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, false);
	gl::draw(mVDSession->getMixTexture(), getWindowBounds());


	/*auto spoutTex = mSpoutIn.receiveTexture();
	if (spoutTex) {
		//  draw the texture and fill the screen
		//gl::draw(spoutTex, getWindowBounds());
		for (auto &warp : mWarps) {
			warp->draw(spoutTex, spoutTex->getBounds());
		}
		if (mVDSettings->mCursorVisible) {
			// Show the user what it is receiving
			gl::ScopedBlendAlpha alpha;
			gl::enableAlphaBlending();
			gl::drawString("Receiving from: " + mSpoutIn.getSenderName(), vec2(toPixels(20), toPixels(20)), Color(1, 1, 1), Font("Verdana", toPixels(24)));
			gl::drawString("fps: " + std::to_string((int)getAverageFps()), vec2(getWindowWidth() - toPixels(100), toPixels(20)), Color(1, 1, 1), Font("Verdana", toPixels(24)));
			gl::drawString("RH click to select a sender", vec2(toPixels(20), getWindowHeight() - toPixels(40)), Color(1, 1, 1), Font("Verdana", toPixels(24)));
		}
	}
	else { */
		// NDI
	if (mReceiver.isReady()) {
		auto meta = mReceiver.getMetadata();
		auto ndiTex = mReceiver.getVideoTexture();

		first = meta.first;
		second = meta.second;
		if (ndiTex.first) {

			Rectf centeredRect = Rectf(ndiTex.first->getBounds()).getCenteredFit(getWindowBounds(), true);
			gl::draw(ndiTex.first, centeredRect);
			for (auto &warp : mWarps) {
				warp->draw(ndiTex.first, ndiTex.first->getBounds());
			}
		}
	//}
	}
	else {
		gl::clear(ColorAf(0.8, 0.0, 0.3));
	}
	// UI
	if (mVDSession->showUI()) {
		mVDUI->Run("UI", (int)getAverageFps());
		if (mVDUI->isReady()) {
		}

	}
}

void prepareSettings(App::Settings *settings)
{
	settings->setWindowSize(1280, 720);
}

CINDER_APP(VD64VizNDISpoutWarpApp, RendererGl, prepareSettings)
