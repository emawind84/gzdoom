#ifndef __GLES_DEBUG_H
#define __GLES_DEBUG_H

#include <string.h>
#include "gles_system.h"
#include "c_cvars.h"
#include "v_video.h"

namespace OpenGLESRenderer
{

class FGLDebug
{
public:
	void Update();

	static void LabelObject(GLenum type, GLuint handle, const char *name);
	static void LabelObjectPtr(void *ptr, const char *name);

	static void PushGroup(const FString &name);
	static void PopGroup();

	// KHR_debug is core (unsuffixed) in GLES 3.2, same entry points as
	// desktop GL 4.3+ core debug output - no ...KHR suffix needed here.
	static bool HasDebugApi() { return (gles.flags & RFL_DEBUG) != 0; }

private:
	void SetupBreakpointMode();
	void UpdateLoggingLevel();
	void OutputMessageLog();

	static bool IsFilteredByDebugLevel(GLenum severity);
	static void PrintMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message);

	static void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);

	static FString SourceToString(GLenum source);
	static FString TypeToString(GLenum type);
	static FString SeverityToString(GLenum severity);

	GLenum mCurrentLevel = 0;
	bool mBreakpointMode = false;
};

}
#endif
