#pragma once

#include <Ogre.h>
#include <OIS/OIS.h>
#include "EditorState.h"

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
#include <CoreFoundation/CoreFoundation.h>
// This function will locate the path to our application on OS X,
// unlike windows you can not rely on the curent working directory
// for locating your configuration files and resources.
std::string macBundlePath()
{
    char path[1024];
    CFBundleRef mainBundle = CFBundleGetMainBundle();    assert(mainBundle);
    CFURLRef mainBundleURL = CFBundleCopyBundleURL(mainBundle);    assert(mainBundleURL);
    CFStringRef cfStringRef = CFURLCopyFileSystemPath( mainBundleURL, kCFURLPOSIXPathStyle);    assert(cfStringRef);
    CFStringGetCString(cfStringRef, path, 1024, kCFStringEncodingASCII);
	CFRelease(mainBundleURL);
    CFRelease(cfStringRef);
    return std::string(path);
}
#endif

class BasisManager : public Ogre::FrameListener, public OIS::MouseListener , public OIS::KeyListener, public Ogre::WindowEventListener
{
public:
	//OIS Input devices
	OIS::InputManager* mInputManager;
	OIS::Keyboard* mKeyboard;
	OIS::Mouse*    mMouse;

    Ogre::Root *mRoot;
    Ogre::Camera* mCamera;
    Ogre::SceneManager* mSceneMgr;
    Ogre::RenderWindow* mWindow;
	Ogre::String mResourcePath;
	bool m_exit; // ������� �� ����� ����������
	size_t mWidth, mHeight; // ������ � ������ ������

	std::vector<BasisState*> mStates; // ������ ���������
	EditorState mEditor;
	//TestState mTest;
	typedef std::vector<std::string> Params;
	Params mParams;

	// ��������� ������ � ������ ����������
	void addCommandParam(const std::string & _param);
	// ���������� ������ ���������� ���������� ������
	inline const Params & getCommandParams() { return mParams; }

	void setWindowCaption(const std::string & _text);

public:
	static BasisManager & getInstance() {static BasisManager instance;return instance;}

	BasisManager();

	void createInput(); // ������� ������� �����
	void destroyInput(); // ������� ������� �����

	void createBasisManager(); // ������� ��������� ����� ������� ����������
	void destroyBasisManager(); // ������� ��� ��������� ������� ����������

	void setupResources(); // ��������� ��� ������� ����������

	bool frameStarted(const Ogre::FrameEvent& evt);
	bool frameEnded(const Ogre::FrameEvent& evt);
	bool mouseMoved( const OIS::MouseEvent &arg );
	bool mousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id );
	bool mouseReleased( const OIS::MouseEvent &arg, OIS::MouseButtonID id );
	bool keyPressed( const OIS::KeyEvent &arg );
	bool keyReleased( const OIS::KeyEvent &arg );

	void changeState(BasisState* state);
	void pushState(BasisState* state);
	void popState();

	void windowResized(Ogre::RenderWindow* rw);
	void windowClosed(Ogre::RenderWindow* rw);

	void setMainWindowIcon(size_t _iconId);

	void dropFile(const std::string & _file);

private:
	void startRendering();

#ifdef WIN32
	// ���� ������� ���������
	static LRESULT CALLBACK windowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT msOldWindowProc;
	// ���������� ������ �������� ����
	size_t mHwnd;
#endif

};
