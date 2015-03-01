#ifndef p1stream_h
#define p1stream_h

#include <stdarg.h>
#include <uv.h>
#include <node.h>
#include <node_object_wrap.h>
#include <functional>

#if __APPLE__
#   include <OpenGL/gl3.h>
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include <IOSurface/IOSurface.h>
#       define HAVE_IOSURFACE
#   endif
#else
#   include <GL/gl.h>
#endif

namespace p1stream {


// ----- PODs -----

typedef int64_t frame_time_t;

struct fraction_t {
    uint32_t num;
    uint32_t den;
};

struct dimensions_t {
    uint32_t width;
    uint32_t height;
};


// ----- Utility types -----

// Access high resolution, monotonic system clock.
int64_t system_time();

// Base for objects that provide a lock. The lock() method can return another
// lockable, if the object is proxying, or nullptr if no lock was necessary.
class lockable {
protected:
    lockable();

public:
    virtual lockable *lock() = 0;
    virtual void unlock();
};

// RAII lock acquisition.
class lock_handle {
private:
    lockable *object_;

public:
    lock_handle(lockable &object);
    lock_handle(lockable *object);
    ~lock_handle();
};

// Lockable implemented with a mutex.
class lockable_mutex : public lockable {
protected:
    uv_mutex_t mutex;

public:
    lockable_mutex();
    ~lockable_mutex();

    virtual lockable *lock() final;
    virtual void unlock() final;
};

// Wrap a thread with its own loop. The loop should call wait() to pause,
// which will return true if destroy() is waiting for the thread to exit.
class threaded_loop : public lockable_mutex {
private:
    uv_thread_t thread;
    uv_cond_t cond;
    std::function<void ()> fn;

    static void thread_cb(void* arg);

public:
    void init(std::function<void ()> fn_);
    void destroy();

    bool wait(uint64_t timeout);
};

// Wrap uv_async with std::function.
class main_loop_callback {
private:
    uv_async_t async;
    std::function<void ()> fn;

    static void async_cb(uv_async_t* handle);

public:
    void init(std::function<void ()> fn_);
    void destroy();

    void send();
};


// ----- Event handling -----

struct event;
class event_buffer;
struct event_buffer_copy;
class buffer_slicer;

// Threads send events to the main thread, which can contain data or log
// messages. Events are queued in a fixed size buffer, then copy'd out by
// the main thread to be emitted one by one.
//
// Locking and signalling is left to the user.

// The event structure as it lives in the buffer.
struct event {
    uint32_t id;    // FourCC identifier.
    uint32_t size;  // Size of data.
    char data[0];   // Struct is followed by data.

    // Number of padding bytes after data.
    inline uint32_t pad();
    // Total size of struct + data + padding.
    inline uint32_t total_size();
    // Return a pointer past this event, accounting for 32-bit alignment.
    event *next();
};

// Event buffer containing consecutive events.
//
// All access to instances of this class should be within a lock. After
// calling one of the emit*() methods, the user should somehow signal the
// main thread to do a copy_out()
class event_buffer final {
private:
    int size_;
    int used_;
    int stalled_;
    char *data_;

public:
    event_buffer(int size = 65536);
    ~event_buffer();

    // The following emit methods all return NULL if the buffer stalled.

    // Emit an empty event.
    event *emit(uint32_t id);
    // Emit an event with a formatted string.
    event *emitv(uint32_t id, const char *format, va_list ap);
    event *emitf(uint32_t id, const char *format, ...)
        __attribute__((format(printf, 3, 4)));
    // Emit an event with raw data. Caller must fill the returned event data.
    event *emitd(uint32_t id, int size);

    // Create a copy of the buffer data and clear the buffer itself.
    event_buffer_copy *copy_out();
};

// Contains a copy of an event_buffer.
struct event_buffer_copy {
    int size;
    int stalled;
    char data[0];

    // Call a JS function for each event in the buffer copy. Call this after
    // releasing the lock that was holding the original event_buffer.
    //
    // The transform function should create a JS value for the given event
    // data. The function is not called for standard events defined below.
    //
    // Calling this method transfers ownership of the entire struct, as it will
    // be tied to a Buffer object.
    void callback(
        v8::Isolate *isolate, v8::Handle<v8::Object> recv, v8::Handle<v8::Function> fn,
        v8::Local<v8::Value> (*transform)(v8::Isolate *isolate, event &ev, buffer_slicer &slicer)
    );
};

// Helper that creates buffer slices. The transform function from
// event_buffer_copy::callback receives a stack allocated instance of this as
// a parameter.
class buffer_slicer {
private:
    v8::Isolate *isolate_;
    v8::Local<v8::Object> buffer_;
    v8::Local<v8::Value> buffer_proto_;
    v8::Local<v8::String> length_sym_;
    v8::Local<v8::String> parent_sym_;

public:
    buffer_slicer(v8::Local<v8::Object> buffer);

    // Returns a new buffer object for the given data and length, which should
    // be part of the original buffer memory area.
    v8::Local<v8::Object> slice(char *data, uint32_t length);
};

// Standard event IDs.

// Logging messages. These contain string data.
#define EV_LOG_TRACE 'ltrc'
#define EV_LOG_DEBUG 'ldbg'
#define EV_LOG_INFO  'linf'
#define EV_LOG_WARN  'lwrn'
#define EV_LOG_ERROR 'lerr'
#define EV_LOG_FATAL 'lfat'

// Generic failure event, which can be used to signal to JS that the object is
// now in a useless state and should be destroyed. Usually preceded by
// relevant log messages. Contains no data.
#define EV_FAILURE 'fail'

// Stall notification. This is automatically created at the end of emit_copy,
// when the buffer stalled flag was set.
#define EV_STALLED 'stal'


// ----- Video types ----

class video_clock_context;
class video_source_context;
class video_hook_context;

// As far as the video mixer is concerned, there's only two threads: the main
// libuv thread and the video clock thread.

// The clock is responsible for the video thread and lock; all of the following
// methods are called with video clock locked if there is one. If there is no
// video clock to lock, there is nothing but the main libuv thread.

// For convenience, the mixer implements lockable as a proxy to the clock if
// there is one, otherwise it's a no-op.

// Sources may introduce their own threads, but will have to manage them on
// their own as well.

class video_mixer : public node::ObjectWrap, public lockable {
protected:
    video_mixer();

    // The texture the mixer renders to.
    GLuint texture_;
#ifdef HAVE_IOSURFACE
    IOSurfaceRef surface_;
#endif

public:
    // Accessors
    GLuint texture();
#ifdef HAVE_IOSURFACE
    IOSurfaceRef surface();
#endif
};

// Base for video clocks. The clock should start a thread and call back
// once per frame. All video processing will happen on this thread.
class video_clock : public node::ObjectWrap, public lockable {
public:
    // When a clock is linked, it should start calling tick().
    virtual void link_video_clock(video_clock_context &ctx) = 0;
    virtual void unlink_video_clock(video_clock_context &ctx) = 0;

    // Get the clock rate. The clock should not call back on tick() unless it
    // can report the video frame rate.
    virtual fraction_t video_ticks_per_second(video_clock_context &ctx) = 0;
};

// Context object passed to the clock.
class video_clock_context {
protected:
    video_clock_context();

    video_clock *clock_;
    video_mixer *mixer_;

public:
    // Accessors.
    video_clock *clock();
    video_mixer *mixer();

    // Tick callback that should be called by the clock.
    void tick(frame_time_t time);
};

// Base for video sources.
class video_source : public node::ObjectWrap {
public:
    // When a source is linked, it will receive produce_video_frame() calls.
    virtual void link_video_source(video_source_context &ctx);
    virtual void unlink_video_source(video_source_context &ctx);

    // Called when the mixer is rendering a frame. Should call one of the
    // render_*() callbacks on the context object.
    virtual void produce_video_frame(video_source_context &ctx) = 0;
};

// Context object passed to the source.
class video_source_context {
protected:
    video_source_context();

    video_source *source_;
    video_mixer *mixer_;

public:
    // Accessors.
    video_source *source();
    video_mixer *mixer();

    // Render callbacks that should be called by sources from within
    // produce_video_frame().
    void render_texture();
    void render_buffer(dimensions_t dimensions, void *data);
#ifdef HAVE_IOSURFACE
    void render_iosurface(IOSurfaceRef surface);
#endif
};

// Base for video hooks. Video hooks are called immediately after a frame is
// rendered, and run on the clock thread.
class video_hook : public node::ObjectWrap {
public:
    // When a hook is linked, it will received video_post_render() calls.
    virtual void link_video_hook(video_hook_context &ctx);
    virtual void unlink_video_hook(video_hook_context &ctx);

    // Called after the mixer has rendered a frame.
    virtual void video_post_render(video_hook_context &ctx) = 0;
};

// Context object passed to the hook.
class video_hook_context {
protected:
    video_hook_context();

    video_hook *hook_;
    video_mixer *mixer_;

public:
    // Accessors.
    video_hook *hook();
    video_mixer *mixer();
};


// ----- Audio types -----

class audio_source_context;

// The audio mixer starts a thread timed with the system clock to do encoding.
// Sources all run their own thread, and can call into `render_buffer()`
// without locking.

class audio_mixer : public node::ObjectWrap, public lockable {
protected:
    audio_mixer();
};

// Base for audio sources.
class audio_source : public node::ObjectWrap {
public:
    // When a source is linked, it should start calling render_buffer().
    virtual void link_audio_source(audio_source_context &ctx) = 0;
    virtual void unlink_audio_source(audio_source_context &ctx) = 0;
};

// Context object passed to audio sources.
class audio_source_context {
protected:
    audio_source_context();

    audio_source *source_;
    audio_mixer *mixer_;

public:
    // Accessors.
    audio_source *source();
    audio_mixer *mixer();

    // Render callback that should be called by sources.
    void render_buffer(int64_t time, float *in, size_t samples);
};


// ----- Inline implementations -----

inline lockable::lockable()
{
}

inline lock_handle::lock_handle(lockable &object)
{
    object_ = object.lock();
}

inline lock_handle::lock_handle(lockable *object)
{
    object_ = object ? object->lock() : nullptr;
}

inline lock_handle::~lock_handle()
{
    if (object_ != nullptr)
        object_->unlock();
}

inline lockable_mutex::lockable_mutex()
{
    if (uv_mutex_init(&mutex))
        abort();
}

inline lockable_mutex::~lockable_mutex()
{
    uv_mutex_destroy(&mutex);
}

inline void threaded_loop::init(std::function<void ()> fn_)
{
    fn = fn_;
    if (uv_cond_init(&cond))
        abort();
    if (uv_thread_create(&thread, thread_cb, this))
        abort();
}

inline void threaded_loop::destroy()
{
    uv_cond_signal(&cond);
    if (uv_thread_join(&thread))
        abort();
    uv_cond_destroy(&cond);
}

inline bool threaded_loop::wait(uint64_t timeout)
{
    return uv_cond_timedwait(&cond, &mutex, timeout) == 0;
}

inline void main_loop_callback::init(std::function<void ()> fn_)
{
    fn = fn_;
    auto loop = uv_default_loop();
    if (uv_async_init(loop, &async, async_cb))
        abort();
}

inline void main_loop_callback::destroy()
{
    uv_close((uv_handle_t *) &async, NULL);
    async.loop = NULL;
}

inline void main_loop_callback::send()
{
    if (uv_async_send(&async))
        abort();
}

inline video_mixer::video_mixer()
{
}

inline GLuint video_mixer::texture()
{
    return texture_;
}

#ifdef HAVE_IOSURFACE
inline IOSurfaceRef video_mixer::surface()
{
    return surface_;
}
#endif

inline video_clock_context::video_clock_context()
{
}

inline video_clock *video_clock_context::clock()
{
    return clock_;
}

inline video_mixer *video_clock_context::mixer()
{
    return mixer_;
}

inline video_source_context::video_source_context()
{
}

inline video_source *video_source_context::source()
{
    return source_;
}

inline video_mixer *video_source_context::mixer()
{
    return mixer_;
}

inline video_hook_context::video_hook_context()
{
}

inline video_hook *video_hook_context::hook()
{
    return hook_;
}

inline video_mixer *video_hook_context::mixer()
{
    return mixer_;
}

inline audio_mixer::audio_mixer()
{
}

inline audio_source_context::audio_source_context()
{
}

inline audio_source *audio_source_context::source()
{
    return source_;
}

inline audio_mixer *audio_source_context::mixer()
{
    return mixer_;
}

inline uint32_t event::pad()
{
    const uint32_t r = size & 0x3;
    return r ? 0x4 - r : 0;
}

inline uint32_t event::total_size()
{
    return sizeof(event) + size + pad();
}

inline event *event::next()
{
    return (event *) (data + size + pad());
}

inline event_buffer::event_buffer(int size)
    : size_(size), used_(0), stalled_(0)
{
    data_ = new char[size_];
}

inline event_buffer::~event_buffer()
{
    delete[] data_;
}

inline event *event_buffer::emit(uint32_t id)
{
    const int data_space = size_ - used_ - sizeof(event);
    event * const ev = (event *) (data_  + used_);

    if (data_space < 0) {
        stalled_++;
        return NULL;
    }
    else {
        ev->id = id;
        ev->size = 0;
        used_ += sizeof(event);
        return ev;
    }
}

inline event *event_buffer::emitv(uint32_t id, const char *format, va_list ap)
{
    const int data_space = size_ - used_ - sizeof(event);
    event * const ev = (event *) (data_ + used_);

    int size = vsnprintf(ev->data, data_space, format, ap);
    if (size < 0) {
        return NULL;
    }
    else if (size > data_space - 1) {
        stalled_++;
        return NULL;
    }
    else {
        ev->id = id;
        ev->size = size;
        used_ += ev->total_size();
        return ev;
    }
}

inline event *event_buffer::emitf(uint32_t id, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    event *ev = emitv(id, format, ap);
    va_end(ap);
    return ev;
}

inline event *event_buffer::emitd(uint32_t id, int size)
{
    const int data_space = size_ - used_ - sizeof(event);
    event * const ev = (event *) (data_ + used_);

    if (size > data_space) {
        stalled_++;
        return NULL;
    }
    else {
        ev->id = id;
        ev->size = size;
        used_ += ev->total_size();
        return ev;
    }
}

inline event_buffer_copy *event_buffer::copy_out()
{
    if (used_ == 0)
        return NULL;

    auto *ret = (event_buffer_copy *) new char[sizeof(event_buffer_copy) + used_];
    memcpy(ret->data, data_, used_);
    ret->size = used_;
    ret->stalled = stalled_;

    used_ = 0;
    stalled_ = 0;

    return ret;
}


}  // namespace p1stream

#endif  // p1stream_h
