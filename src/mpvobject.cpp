#include "mpvobject.h"

#include <stdexcept>
#include <clocale>

#include <QObject>
#include <QtGlobal>
#include <QOpenGLContext>

#include <QGuiApplication>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QQuickView>

static void *get_proc_address(void *ctx, const char *name) {
	(void)ctx;
	QOpenGLContext *glctx = QOpenGLContext::currentContext();
	if (!glctx)
		return NULL;
	return (void *)glctx->getProcAddress(QByteArray(name));
}

MpvRenderer::MpvRenderer(mpv::qt::Handle a_mpv, mpv_opengl_cb_context *a_mpv_gl)
	: mpv(a_mpv)
	, mpv_gl(a_mpv_gl)
	, window(0)
	, size()
{
	int r = mpv_opengl_cb_init_gl(mpv_gl, NULL, get_proc_address, NULL);
	if (r < 0)
		throw std::runtime_error("could not initialize OpenGL");
}

MpvRenderer::~MpvRenderer()
{
	// Until this call is done, we need to make sure the player remains
	// alive. This is done implicitly with the mpv::qt::Handle instance
	// in this class.
	mpv_opengl_cb_uninit_gl(mpv_gl);
}

void MpvRenderer::paint()
{
	window->resetOpenGLState();

	// This uses 0 as framebuffer, which indicates that mpv will render directly
	// to the frontbuffer. Note that mpv will always switch framebuffers
	// explicitly. Some QWindow setups (such as using QQuickWidget) actually
	// want you to render into a FBO in the beforeRendering() signal, and this
	// code won't work there.
	// The negation is used for rendering with OpenGL's flipped coordinates.
	mpv_opengl_cb_draw(mpv_gl, 0, size.width(), -size.height());

	window->resetOpenGLState();
}

static void wakeup(void *ctx)
{
	QMetaObject::invokeMethod((MpvObject*)ctx, "on_mpv_events", Qt::QueuedConnection);
}

MpvObject::MpvObject(QQuickItem * parent)
	: QQuickItem(parent)
	, mpv_gl(0)
	, renderer(0)
	, killOnce(false)
	, m_enableAudio(true)
	, m_duration(0)
	, m_position(0)
	, m_isPlaying(false)
{
	mpv = mpv::qt::Handle::FromRawHandle(mpv_create());
	if (!mpv)
		throw std::runtime_error("could not create mpv context");

	mpv_set_option_string(mpv, "terminal", "yes");
	mpv_set_option_string(mpv, "msg-level", "all=warn,ao/alsa=error"); // all=no OR all=v

	if (mpv_initialize(mpv) < 0)
		throw std::runtime_error("could not initialize mpv context");


	mpv::qt::set_option_variant(mpv, "audio-client-name", "mpvz");

	// Make use of the MPV_SUB_API_OPENGL_CB API.
	mpv::qt::set_option_variant(mpv, "vo", "opengl-cb");

	// Not sure how
	// mpv::qt::set_option_variant(mpv, "vo", "opengl-cb:interpolation");
	// mpv_set_option_string(mpv, "video-sync", "display-resample");
	// mpv::qt::set_option_variant(mpv, "vf", "lavfi=\"fps=fps=60:round=down\"");

	// Request hw decoding by default
	mpv::qt::set_option_variant(mpv, "hwdec", "auto");

	// Setup the callback that will make QtQuick update and redraw if there
	// is a new video frame. Use a queued connection: this makes sure the
	// doUpdate() function is run on the GUI thread.
	mpv_gl = (mpv_opengl_cb_context *)mpv_get_sub_api(mpv, MPV_SUB_API_OPENGL_CB);
	if (!mpv_gl)
		throw std::runtime_error("OpenGL not compiled in");
	mpv_opengl_cb_set_update_callback(mpv_gl, MpvObject::on_update, (void *)this);
	connect(this, &MpvObject::mpvUpdated, this, &MpvObject::doUpdate,
			Qt::QueuedConnection);

	connect(this, &QQuickItem::windowChanged,
			this, &MpvObject::handleWindowChanged);

	WATCH_PROP_BOOL("idle")
	WATCH_PROP_BOOL("mute")
	WATCH_PROP_BOOL("pause")
	WATCH_PROP_BOOL("paused-for-cache")
	WATCH_PROP_BOOL("seekable")
	WATCH_PROP_INT("chapter")
	WATCH_PROP_INT("chapter-list/count")
	WATCH_PROP_INT("decoder-frame-drop-count")
	WATCH_PROP_INT("dheight")
	WATCH_PROP_INT("dwidth")
	WATCH_PROP_INT("estimated-frame-count")
	WATCH_PROP_INT("estimated-frame-number")
	WATCH_PROP_INT("frame-drop-count")
	WATCH_PROP_INT("playlist-pos")
	WATCH_PROP_INT("playlist/count")
	WATCH_PROP_INT("vo-delayed-frame-count")
	WATCH_PROP_INT("volume")
	WATCH_PROP_DOUBLE("audio-bitrate")
	WATCH_PROP_DOUBLE("avsync")
	WATCH_PROP_DOUBLE("container-fps")
	WATCH_PROP_DOUBLE("duration")
	WATCH_PROP_DOUBLE("estimated-display-fps")
	WATCH_PROP_DOUBLE("estimated-vf-fps")
	WATCH_PROP_DOUBLE("fps")
	WATCH_PROP_DOUBLE("speed")
	WATCH_PROP_DOUBLE("time-pos")
	WATCH_PROP_DOUBLE("video-bitrate")
	WATCH_PROP_STRING("audio-codec")
	WATCH_PROP_STRING("audio-codec-name")
	WATCH_PROP_STRING("filename")
	WATCH_PROP_STRING("file-format")
	WATCH_PROP_STRING("file-size")
	WATCH_PROP_STRING("format")
	WATCH_PROP_STRING("hwdec")
	WATCH_PROP_STRING("hwdec-current")
	WATCH_PROP_STRING("hwdec-interop")
	WATCH_PROP_STRING("media-title")
	WATCH_PROP_STRING("path")
	WATCH_PROP_STRING("video-codec")
	WATCH_PROP_STRING("video-format")

	connect(this, &MpvObject::idleChanged,
			this, &MpvObject::updateState);
	connect(this, &MpvObject::pausedChanged,
			this, &MpvObject::updateState);
	
	mpv_set_wakeup_callback(mpv, wakeup, this);
}

MpvObject::~MpvObject()
{
	if (mpv_gl)
		mpv_opengl_cb_set_update_callback(mpv_gl, NULL, NULL);
}

void MpvObject::handleWindowChanged(QQuickWindow *win)
{
	if (!win)
		return;
	connect(win, &QQuickWindow::beforeSynchronizing,
			this, &MpvObject::sync, Qt::DirectConnection);
	connect(win, &QQuickWindow::sceneGraphInvalidated,
			this, &MpvObject::cleanup, Qt::DirectConnection);
	connect(win, &QQuickWindow::frameSwapped,
			this, &MpvObject::swapped, Qt::DirectConnection);
	win->setClearBeforeRendering(false);
}

void MpvObject::sync()
{
	if (killOnce)
		cleanup();
	killOnce = false;

	if (!renderer) {
		renderer = new MpvRenderer(mpv, mpv_gl);
		connect(window(), &QQuickWindow::beforeRendering,
				renderer, &MpvRenderer::paint, Qt::DirectConnection);
	}
	renderer->window = window();
	renderer->size = window()->size() * window()->devicePixelRatio();
}

void MpvObject::swapped()
{
	mpv_opengl_cb_report_flip(mpv_gl, 0);
}

void MpvObject::cleanup()
{
	if (renderer) {
		delete renderer;
		renderer = 0;
	}
}

void MpvObject::on_update(void *ctx)
{
	MpvObject *self = (MpvObject *)ctx;
	emit self->mpvUpdated();
}

// connected to mpvUpdated(); signal makes sure it runs on the GUI thread
void MpvObject::doUpdate()
{
	window()->update();
}

void MpvObject::command(const QVariant& params)
{
	mpv::qt::command_variant(mpv, params);
}

void MpvObject::setProperty(const QString& name, const QVariant& value)
{
	mpv::qt::set_property_variant(mpv, name, value);
}

QVariant MpvObject::getProperty(const QString &name) const
{
	return mpv::qt::get_property_variant(mpv, name);
}

void MpvObject::setOption(const QString& name, const QVariant& value)
{
	mpv::qt::set_option_variant(mpv, name, value);
}

void MpvObject::reinitRenderer()
{
	// Don't make it stop playback if the VO dies.
	mpv_set_option_string(mpv, "stop-playback-on-init-failure", "no");
	// Make it recreate the renderer, which involves calling
	// mpv_opengl_cb_uninit_gl() (which is the thing we want to test).
	killOnce = true;
	window()->update();
}


void MpvObject::on_mpv_events()
{
	// Process all events, until the event queue is empty.
	while (mpv) {
		mpv_event *event = mpv_wait_event(mpv, 0);
		if (event->event_id == MPV_EVENT_NONE) {
			break;
		}
		handle_mpv_event(event);
	}
}

void MpvObject::handle_mpv_event(mpv_event *event)
{
	// See: https://github.com/mpv-player/mpv/blob/master/libmpv/client.h
	// See: https://github.com/mpv-player/mpv/blob/master/player/lua.c#L471

	switch (event->event_id) {
	case MPV_EVENT_START_FILE: {
		Q_EMIT fileStarted();
		break;
	}
	case MPV_EVENT_END_FILE: {
		mpv_event_end_file *eef = (mpv_event_end_file *)event->data;
		const char *reason;
		switch (eef->reason) {
		case MPV_END_FILE_REASON_EOF: reason = "eof"; break;
		case MPV_END_FILE_REASON_STOP: reason = "stop"; break;
		case MPV_END_FILE_REASON_QUIT: reason = "quit"; break;
		case MPV_END_FILE_REASON_ERROR: reason = "error"; break;
		case MPV_END_FILE_REASON_REDIRECT: reason = "redirect"; break;
		default:
			reason = "unknown";
		}
		Q_EMIT fileEnded(QString(reason));
		break;
	}
	case MPV_EVENT_FILE_LOADED: {
		Q_EMIT fileLoaded();
		break;
	}
	case MPV_EVENT_PROPERTY_CHANGE: {
		mpv_event_property *prop = (mpv_event_property *)event->data;
		if (prop->format == MPV_FORMAT_DOUBLE) {
			if (strcmp(prop->name, "time-pos") == 0) {
				double time = *(double *)prop->data;
				m_position = time;
				Q_EMIT positionChanged(time);
			} else if (strcmp(prop->name, "duration") == 0) {
				double time = *(double *)prop->data;
				m_duration = time;
				Q_EMIT durationChanged(time);
			}
			else if HANDLE_PROP_DOUBLE("audio-bitrate", audioBitrate)
			else if HANDLE_PROP_DOUBLE("avsync", avsync)
			else if HANDLE_PROP_DOUBLE("container-fps", containerFps)
			else if HANDLE_PROP_DOUBLE("estimated-display-fps", estimatedDisplayFps)
			else if HANDLE_PROP_DOUBLE("estimated-vf-fps", estimatedVfFps)
			else if HANDLE_PROP_DOUBLE("fps", fps)
			else if HANDLE_PROP_DOUBLE("speed", speed)
			else if HANDLE_PROP_DOUBLE("video-bitrate", videoBitrate)

		} else if (prop->format == MPV_FORMAT_FLAG) {
			if HANDLE_PROP_BOOL("idle", idle)
			else if HANDLE_PROP_BOOL("mute", muted)
			else if HANDLE_PROP_BOOL("pause", paused)
			else if HANDLE_PROP_BOOL("paused-for-cache", pausedForCache)
			else if HANDLE_PROP_BOOL("seekable", seekable)

		} else if (prop->format == MPV_FORMAT_STRING) {
			if HANDLE_PROP_STRING("audio-codec", audioCodec)
			else if HANDLE_PROP_STRING("audio-codec-name", audioCodecName)
			else if HANDLE_PROP_STRING("filename", filename)
			else if HANDLE_PROP_STRING("file-format", fileFormat)
			else if HANDLE_PROP_STRING("file-size", fileSize)
			else if HANDLE_PROP_STRING("format", format)
			else if HANDLE_PROP_STRING("hwdec", hwdec)
			else if HANDLE_PROP_STRING("hwdec-current", hwdecCurrent)
			else if HANDLE_PROP_STRING("hwdec-interop", hwdecInterop)
			else if HANDLE_PROP_STRING("media-title", mediaTitle)
			else if HANDLE_PROP_STRING("path", path)
			else if HANDLE_PROP_STRING("video-codec", videoCodec)
			else if HANDLE_PROP_STRING("video-format", videoFormat)


		} else if (prop->format == MPV_FORMAT_INT64) {
			if HANDLE_PROP_INT("chapter", chapter)
			else if HANDLE_PROP_INT("chapter-list/count", chapterListCount)
			else if HANDLE_PROP_INT("decoder-frame-drop-count", decoderFrameDropCount)
			else if HANDLE_PROP_INT("dwidth", dwidth)
			else if HANDLE_PROP_INT("dheight", dheight)
			else if HANDLE_PROP_INT("estimated-frame-count", estimatedFrameCount)
			else if HANDLE_PROP_INT("estimated-frame-number", estimatedFrameNumber)
			else if HANDLE_PROP_INT("frame-drop-count", frameDropCount)
			else if HANDLE_PROP_INT("playlist-pos", playlistPos)
			else if HANDLE_PROP_INT("playlist/count", playlistCount)
			else if HANDLE_PROP_INT("vo-delayed-frame-count", voDelayedFrameCount)
			else if HANDLE_PROP_INT("volume", volume)
				
		}
		break;
	}
	default: ;
		// Ignore uninteresting or unknown events.
	}
}

void MpvObject::play()
{
	if (idle() && playlistCount() >= 1) { // File has finished playing.
		set_playlistPos(playlistPos()); // Reload and play file again.
	}
	if (!isPlaying()) {
		set_paused(false);
	}
}

void MpvObject::pause()
{
	if (isPlaying()) {
		set_paused(true);
	}
}

void MpvObject::playPause()
{
	if (isPlaying()) {
		pause();
	} else {
		play();
	}
}

void MpvObject::seek(double pos)
{
	command(QVariantList() << "seek" << pos << "absolute");
}

void MpvObject::loadFile(QVariant urls)
{
	command(QVariantList() << "loadfile" << urls);
}


void MpvObject::updateState()
{
	bool isNowPlaying = !idle() && !paused();
	if (m_isPlaying != isNowPlaying) {
		m_isPlaying = isNowPlaying;
		emit isPlayingChanged(m_isPlaying);
	}
}

