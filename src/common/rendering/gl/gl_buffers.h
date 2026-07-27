#pragma once

#include "buffers.h"
#include "gl_load.h"

#ifdef _MSC_VER
// silence bogus warning C4250: 'GLVertexBuffer': inherits 'GLBuffer::GLBuffer::SetData' via dominance
// According to internet infos, the warning is erroneously emitted in this case.
#pragma warning(disable:4250) 
#endif

namespace OpenGLRenderer
{

class GLBuffer : virtual public IBuffer
{
protected:
	const int mUseType;
	unsigned int mBufferId;
	int mAllocationSize = 0;
	bool mPersistent = false;
	bool nomap = true;
	GLsync mGLSync = 0;
#ifdef MALI_BUFFER_WORKAROUND
	// ARM Mali GLES drivers are unreliable with glMapBufferRange/glUnmapBuffer on
	// these buffers, producing "buffer is already mapped" errors at draw time.
	// This has not been observed on Quest's Adreno GPU, so this workaround is
	// gated on its own macro (defined only for the Mali/R36S build, e.g. the
	// desktop CMake path - NOT by Android_src.mk) rather than the general
	// __MOBILE__ flag, which both Quest and R36S define. Real GL mapping stays
	// in effect wherever MALI_BUFFER_WORKAROUND isn't defined. Mirrors the
	// OpenGL ES backend's own approach (gles_buffers.cpp, gles.useMappedBuffers).
	char *mShadowBuffer = nullptr;
#endif

	GLBuffer(int usetype);
	~GLBuffer();
	void SetData(size_t size, const void *data, BufferUsageType usage) override;
	void SetSubData(size_t offset, size_t size, const void *data) override;
	void Upload(size_t start, size_t size) override;
	void Map() override;
	void Unmap() override;
	void Resize(size_t newsize) override;
	void *Lock(unsigned int size) override;
	void Unlock() override;

	void GPUDropSync();
	void GPUWaitSync();
public:
	void Bind();
};


class GLVertexBuffer : public IVertexBuffer, public GLBuffer
{
	// If this could use the modern (since GL 4.3) binding system, things would be simpler... :(
	struct GLVertexBufferAttribute
	{
		int bindingpoint;
		int format;
		bool normalize;
		bool integerType;
		int size;
		int offset;
	};

	int mNumBindingPoints;
	GLVertexBufferAttribute mAttributeInfo[VATTR_MAX] = {};	// Thanks to OpenGL's state system this needs to contain info about every attribute that may ever be in use throughout the entire renderer.
	size_t mStride = 0;

public:
	GLVertexBuffer();
	void SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs) override;
	void Bind(int *offsets);
};

class GLIndexBuffer : public IIndexBuffer, public GLBuffer
{
public:
	GLIndexBuffer();
};

class GLDataBuffer : public IDataBuffer, public GLBuffer
{
	int mBindingPoint;
public:
	GLDataBuffer(int bindingpoint, bool is_ssbo);
	void BindRange(FRenderState* state, size_t start, size_t length);
	void BindBase();
};

}