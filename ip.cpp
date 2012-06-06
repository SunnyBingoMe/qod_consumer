#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "consumer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/ip.h>
#include <SDL/SDL.h>
#include <glutils.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <math.h>
#include <vector>
#include "gtk_pie_chart.h"

#define BUFFER_SIZE 150000 /* nr of packages, not bytes */

static GdkGLConfig* glconfig = NULL;
static GtkWidget* window = NULL;
static GtkWidget* drawingarea = NULL;
static GtkWidget* framerate = NULL;
static GdkColor tcp_color = {0, 0xffff, 0, 0};
static GdkColor udp_color = {0, 0, 0xffff, 0};
static GdkColor icmp_color = {0, 0, 0, 0xffff};
static GdkColor arp_color = {0, 0xffff, 0, 0xffff};
static GdkColor other_color = {0, 0xffff, 0xffff, 0xffff};
static GLuint fbo[2] = {0,0}, texture[2] = {0,0};
static GLuint sp;
static int cur = 0;
static consumer_thread_t con;

static struct {
  int tcp;
  int udp;
  int icmp;
  int arp;
  int other;
} stat_transport = {0,0,0,0};

class Framerate2: public Framerate {
public:
  Framerate2(int max_dt, int fps): Framerate(max_dt, fps){}
  
  virtual void refresh_framerate(){
    if ( framerate ){
      char buf[100];
      snprintf(buf, 100, "Framerate: %d avg: %.1f", current_fps(), average_fps());
      gtk_label_set_text((GtkLabel*)framerate, buf);
    }
  }
};

class PieChart {
public:
  class Segment {
  public:
    Segment(double sweep, GdkColor* color, const char* name)
      : _sweep(0.0)
      , _name(NULL){

      set_sweep(sweep);
      set_color(color);
      set_name(name);
    }

    ~Segment(){
      free(_name);
    }

    double sweep(){ return _sweep; }
    void set_sweep(double value){ _sweep = value; }

    GdkColor* color(){ return &_color; }
    void set_color(GdkColor* color){ memcpy(&_color, color, sizeof(GdkColor)); }      

    const char* name(){ return _name; }
    void set_name(const char* name){ free(_name); _name = strdup(name); }

  private:
    double _sweep;
    GdkColor _color;
    char* _name;
  };
  
private:
  typedef std::vector<Segment*> segment_vector_t;
  
public:
  
  PieChart(){
    
  }

  ~PieChart(){
    for ( segment_vector_t::iterator it = segment.begin(); it != segment.end(); ++it ){
      delete (*it);
    }
  }

  Segment* add_segment(double sweep, GdkColor* color, const char* name){
    Segment* s = new Segment(sweep, color, name);
    segment.push_back(s);
    return s;
  }

  void render(cairo_t* cr){
    cairo_save(cr);

    double start = 0.0;

    double width = 256;
    double height = 256;
    double radius = fmin( width, height )*0.28;

    cairo_set_line_width (cr, 1.0);
    for ( segment_vector_t::iterator it = segment.begin(); it != segment.end(); ++it ){
      Segment* s = *it;
      
      /* this rendering code comes from gtk_pie_chart but I am not using that
       * widget as it has several limitations, but especially because it is not
       * trivial to update the segments in realtime. */
      /* Copyright (C) 2008 Aron Rubin */
      /* License: LGPLv2 */
      /* http://rubinium.org/blog/archives/2008/06/10/gtk-pie-chart-widget/ */

      float end = start +  s->sweep() * (2.0*M_PI);
      float mid = (start + end)*0.5;
 
      cairo_move_to( cr, width/2, height/2 );
      cairo_line_to( cr, width/2 + radius*cos( start ), height/2 + radius*sin( start ) );
      cairo_arc( cr, width/2, height/2, radius, start, end );
      cairo_close_path( cr );
      
      cairo_set_line_width( cr, 1.0 );
      cairo_set_source_rgba( cr, s->color()->red, s->color()->green, s->color()->blue, 1.0);
      cairo_fill_preserve( cr );
      cairo_set_source_rgba( cr, 0, 0, 0, 1.0 );
      cairo_stroke( cr );
      
      cairo_move_to( cr, width/2 + 0.5*radius*cos( mid ), height/2 + 0.5*radius*sin( mid ) );
      cairo_line_to( cr, width/2 + 1.3*radius*cos( mid ), height/2 + 1.3*radius*sin( mid ) );
      if( mid > M_PI_2 && mid < (M_PI_2 + M_PI) )
	cairo_line_to( cr, width/6,   height/2 + 1.3*radius*sin( mid ) );
      else
	cairo_line_to( cr, width*5/6, height/2 + 1.3*radius*sin( mid ) );
      cairo_stroke( cr );
      cairo_restore( cr );
      
      cairo_save( cr );
      if( mid > M_PI_2 && mid < (M_PI_2 + M_PI) ) {
	cairo_text_extents_t text_extents = { 0, 0, 0, 0, 0, 0 };
	cairo_text_extents( cr, s->name(), &text_extents );
	cairo_move_to( cr, width/6 - text_extents.width, height/2 + 1.3*radius*sin( mid ) + 4.0 );
      } else {
	cairo_move_to( cr, width*5/6, height/2 + 1.3*radius*sin( mid ) + 4.0 );
      }
      cairo_set_source_rgba( cr, 0, 0, 0, 1.0 );
      cairo_select_font_face( cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL );
      cairo_set_font_size( cr, 10.0 );
      cairo_show_text( cr, s->name() );
      
      start += (end-start);
    }

    cairo_restore(cr);
  }

private:
  segment_vector_t segment;
};

void generate_fbo(int w, int h){
  /* Setup framebuffer */
  if ( fbo[0] == 0 ){
    glGenFramebuffers(2, fbo);
    glGenTextures(2, texture);
  }
  
  for ( int i = 0; i < 2; i++ ){
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
    glBindTexture(GL_TEXTURE_2D, texture[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[i], 0);
    
    GLenum err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if ( err != GL_FRAMEBUFFER_COMPLETE ){
      fprintf(stderr, "Framebuffer incomplete\n");
    }
    
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

extern "C" {
  gboolean color_sel_tcp_color_set_cb(GtkColorButton* widget, gpointer user_data){
    gtk_color_button_get_color(widget, &tcp_color);
    return FALSE;
  }

  gboolean color_sel_udp_color_set_cb(GtkColorButton* widget, gpointer user_data){
    gtk_color_button_get_color(widget, &udp_color);
    return FALSE;
  }

  gboolean color_sel_icmp_color_set_cb(GtkColorButton* widget, gpointer user_data){
    gtk_color_button_get_color(widget, &icmp_color);
    return FALSE;
  }

  gboolean drawingarea1_configure_event_cb(GtkWidget* widget, gpointer user_data){
    printf("configure\n");
    GdkGLContext *glcontext = gtk_widget_get_gl_context (widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable (widget);
    
    /*** OpenGL BEGIN ***/
    if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext))
      return FALSE;
    
    glViewport (0, 0, widget->allocation.width, widget->allocation.height);

    /* setup othogonal projection matrix with (0,0) in the upper left corner and with a size of 1x1: */
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1.0, 1.0);
    glScalef(1, -1, 1);
    glTranslated(0, -1, 0);
    
    if ( fbo[0] > 0 ){
      generate_fbo(widget->allocation.width, widget->allocation.height);
    }

    gdk_gl_drawable_gl_end (gldrawable);
    /*** OpenGL END ***/
    
    return FALSE;
  }
  
  void drawingarea1_realize_cb(GtkWidget* widget, gpointer user_data){
    GdkGLContext *glcontext = gtk_widget_get_gl_context (widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable (widget);

    /*** OpenGL BEGIN ***/
    if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext))
      return;

    GLenum err = glewInit();
    if ( err != GLEW_OK ){
      fprintf(stderr, "Failed to init glew: %s\n", glewGetErrorString(err));
    }

    /* General OpenGL setup */
    glClearColor(1,0,1,0);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(3);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glShadeModel(GL_SMOOTH);

    generate_fbo(widget->allocation.width, widget->allocation.height);

    GLuint vp = compile(GL_VERTEX_SHADER, "../ip_vp.glsl");
    GLuint fp = compile(GL_FRAGMENT_SHADER, "../ip_fp.glsl");
    sp = glCreateProgram();
  
    glAttachShader(sp, vp);
    glAttachShader(sp, fp);
    glLinkProgram(sp);

    glUseProgram(sp);

    GLint sampler = glGetUniformLocation(sp, "texture");
    glUniform1i(sampler, 0);

    glUseProgram(0);

    gdk_gl_drawable_gl_end (gldrawable);
    /*** OpenGL END ***/
  }

  gboolean drawingarea1_expose_event_cb(GtkWidget* widget, gpointer user_data){
    static Framerate2 framerate(100, 0);
    float dt = framerate.update();

    GdkGLContext *glcontext = gtk_widget_get_gl_context (widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable (widget);
    
    /*** OpenGL BEGIN ***/
    if (!gdk_gl_drawable_gl_begin (gldrawable, glcontext))
      return FALSE;
    
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    gl_check_error();

    glBindFramebuffer(GL_FRAMEBUFFER, fbo[cur]);
    glUseProgram(sp);
    
    glBindTexture(GL_TEXTURE_2D, texture[1-cur]);

    glBegin(GL_QUADS);
    glVertex3f(0,0,0);
    glVertex3f(1,0,0);
    glVertex3f(1,1,0);
    glVertex3f(0,1,0);
    glEnd();
    
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    struct cap_header head;
    struct frame_t frame;
    int stream_id;
    //char src[100];
    //char dst[100];

    glPushAttrib(GL_COLOR_BUFFER_BIT);
    glBlendFunc(GL_ONE, GL_ONE);

    int n = 0;
    while ( consumer_thread_poll(con, &head, &frame, &stream_id) ){
      if ( n++ > BUFFER_SIZE*2 ){ /* packets max */
	printf("breaking\n");
	break;
      }
      
      GdkColor* color;

      if ( (frame.type&PACKET_IP) && (frame.type&TRANSPORT_TCP) ){
	stat_transport.tcp++;
	color = &tcp_color;
      } else if ( (frame.type&PACKET_IP) && (frame.type&TRANSPORT_UDP) ){
	stat_transport.udp++;
	color = &udp_color;
      } else if ( frame.type & PACKET_ICMP ){
	stat_transport.icmp++;
	color = &icmp_color;
      } else if ( frame.type & PACKET_ARP ){
	stat_transport.arp++;
	continue;
      } else {
	stat_transport.other++;
	continue;
      }

      glColor4f((float)color->red/0xffff, (float)color->green/0xffff, (float)color->blue/0xffff, 1.0f);
      
      //strcpy(src, inet_ntoa(frame.ip->ip_src));
      //strcpy(dst, inet_ntoa(frame.ip->ip_dst));

      union {
	uint32_t i;
	unsigned char c[4];
      } src, dst;
      
      src.i = ntohl(frame.ip->ip_src.s_addr);
      dst.i = ntohl(frame.ip->ip_dst.s_addr);
      float y = 1.0;
      if ( stream_id == 1 ){
	y = 0.0;
      }

      glBegin(GL_TRIANGLES);
      glVertex2f(((float)src.c[3]) / 255 - 0.006, y);
      glVertex2f(((float)dst.c[3]) / 255, 1.0 - y);
      glVertex2f(((float)src.c[3]) / 255 + 0.006, y);
      glEnd();
    }

    glPopAttrib();

    glColor4f(1,1,1,1);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glClearColor(1,0,1,1);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindTexture(GL_TEXTURE_2D, texture[cur]);
    /* render a fullscreen quad */
    glBegin(GL_QUADS);
    glTexCoord2f(0,1); glVertex3f(0,0,0);
    glTexCoord2f(1,1); glVertex3f(1,0,0);
    glTexCoord2f(1,0); glVertex3f(1,1,0);
    glTexCoord2f(0,0); glVertex3f(0,1,0);
    glEnd();

    cur = 1 - cur; /* swap fbo */

    if (gdk_gl_drawable_is_double_buffered (gldrawable))
      gdk_gl_drawable_swap_buffers (gldrawable);
    else
      glFlush ();
    
    unsigned int retraceCount;
    glXGetVideoSyncSGI(&retraceCount);
    glXWaitVideoSyncSGI(2, (retraceCount+1)%2, &retraceCount);

    gdk_gl_drawable_gl_end (gldrawable);
    /*** OpenGL END ***/

  return FALSE;
  }

  gboolean drawingarea2_expose_event_cb(GtkWidget* widget, gpointer user_data){
    const long int sum =
      stat_transport.tcp +
      stat_transport.udp +
      stat_transport.icmp +
      stat_transport.arp +
      stat_transport.other;
    const int threshold = sum  / 50; /* 2% */
    const float other = (
      stat_transport.tcp  <= threshold ? stat_transport.tcp  : 0 +
      stat_transport.tcp  <= threshold ? stat_transport.udp  : 0 +
      stat_transport.tcp  <= threshold ? stat_transport.icmp : 0 +
      stat_transport.tcp  <= threshold ? stat_transport.arp  : 0 +
      stat_transport.other
    ) / sum;

    float tcp   = stat_transport.tcp  > threshold ? (float)stat_transport.tcp   / sum : -0.0;
    float udp   = stat_transport.udp  > threshold ? (float)stat_transport.udp   / sum : -0.0;
    float icmp  = stat_transport.icmp > threshold ? (float)stat_transport.icmp  / sum : -0.0;
    float arp   = stat_transport.arp  > threshold ? (float)stat_transport.arp   / sum : -0.0;

    cairo_t* cr = gdk_cairo_create (widget->window);

    /* Set surface to opaque color (r, g, b) */
    cairo_set_source_rgba (cr, 1, 1, 1, 0);
    cairo_paint (cr);

    char buf[100];

    PieChart transport;
    if ( tcp  > 0.0 ){
      snprintf(buf, 100, "TCP %.1f%% ", tcp*100);
      transport.add_segment(tcp, &tcp_color, buf);
    }
    if ( udp  > 0.0 ){
      snprintf(buf, 100, "UDP %.1f%% ", udp*100);
      transport.add_segment(udp, &udp_color, buf);
    }
    if ( icmp > 0.0 ){
      snprintf(buf, 100, "ICMP %.1f%% ", icmp*100);
      transport.add_segment(icmp, &icmp_color, buf);
    }
    if ( arp  > 0.0 ){
      snprintf(buf, 100, "ARP %.1f%% ", arp*100);
      transport.add_segment(arp, &arp_color, buf);
    }
    snprintf(buf, 100, "Other %.1f%% ", other*100);
    transport.add_segment(other, &other_color, buf);

    transport.render(cr);

    cairo_destroy (cr);
    return FALSE;
  }

}

static gboolean timer(gpointer user_data){
  GtkWidget* drawingarea = GTK_WIDGET (user_data);
  if ( !(drawingarea->window && gtk_widget_is_drawable(drawingarea)) ){
    return TRUE;
  }
  gdk_window_invalidate_rect (drawingarea->window, &drawingarea->allocation, FALSE);
  //gdk_window_process_updates (drawingarea->window, FALSE);
  return TRUE;
}

int main(int argc, char **argv){
  unsigned long long pktCount = 0;

  gtk_init(&argc, &argv);
  gtk_gl_init (&argc, &argv);

  glconfig = gdk_gl_config_new_by_mode (static_cast<GdkGLConfigMode>(
    GDK_GL_MODE_RGB    |
    GDK_GL_MODE_DEPTH  |
    GDK_GL_MODE_DOUBLE));

  if ( !glconfig ){
    g_print ("*** Cannot find the double-buffered visual.\n");
    g_print ("*** Trying single-buffered visual.\n");
    
    /* Try single-buffered visual */
    glconfig = gdk_gl_config_new_by_mode (static_cast<GdkGLConfigMode>(
      GDK_GL_MODE_RGB   |
      GDK_GL_MODE_DEPTH));

    if ( !glconfig ){
      g_print ("*** No appropriate OpenGL-capable visual found.\n");
      exit (1);
    }
  }

  {
    GError *err = NULL;
    GtkBuilder* builder = gtk_builder_new();
    if ( !gtk_builder_add_from_file(builder, "ip.xml", &err) ){
      fprintf(stderr, "%s: %s\n", argv[0], err->message);
      exit(1);
    }
    gtk_builder_connect_signals (builder, NULL);
    window = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
    drawingarea = GTK_WIDGET(gtk_builder_get_object(builder, "drawingarea1"));
    framerate = GTK_WIDGET(gtk_builder_get_object(builder, "framerate"));

    gtk_widget_set_gl_capability (drawingarea, glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE);
    g_timeout_add (1000 / 15, timer, drawingarea);

    g_object_unref ( G_OBJECT(builder) );
  }

  struct filter filter;
  filter_from_argv(&argc, argv, &filter);

  long ret;
  timepico delay = {2, 0};
  consumer_thread_init(&con, BUFFER_SIZE, &delay);
  (ret=consumer_thread_add_stream(con, "01:00:00:00:00:01", 1, "br0", 0, &filter)) == 0 || printf("strean 1 failed: %s\n", caputils_error_string(ret));
  (ret=consumer_thread_add_stream(con, "01:00:00:00:00:02", 1, "br0", 0, &filter)) == 0 || printf("stream 2 failed: %s\n", caputils_error_string(ret));
  (ret=consumer_thread_add_stream(con, "01:00:00:00:00:03", 1, "br0", 0, &filter)) == 0 || printf("stream 3 failed: %s\n", caputils_error_string(ret));

  gtk_widget_show_all(window);
  gtk_main();

  consumer_thread_destroy(con);
  filter_close(&filter);

  fprintf(stderr, "There was a total of %lld pkts that matched the filter.\n", pktCount);
  return 0;
}
