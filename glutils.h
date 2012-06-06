#ifndef CONSUMER_OPENGL_UTILS
#define CONSUMER_OPENGL_UTILS

#include <GL/glew.h>
#include <GL/glxew.h>
#include <GL/gl.h>

float getTimef();

class Framerate {
public:
  Framerate(float max_dt, unsigned int fps);
  ~Framerate();

  void set_fps(unsigned int n);
  float update();

  virtual void refresh_framerate(){
    /* do nothing */
  }

  unsigned int current_fps() const {
    return _current_fps;
  }

  float average_fps() const {
    return _average_fps;
  }

private:
  void calculate_average_fps(unsigned int sample);

  float _timestamp;	       /* last timestamp */
  float _fps_stamp;	       /* last framerate stamp */
  float _max_dt;		       /* largest dt */
  
  unsigned int _frame_counter;	/* current amount of frames */
  unsigned int _frame_iteration; /* incremental avarage must keep track of
				  * how many times it has been calculated */
  unsigned int _current_fps;     /* current fps */
  float	     _average_fps;     /* avarage fps */
  
  float _timebase;	       /* the desired time each frame should take */
};

void print_log(GLuint shader);
GLuint compile(GLuint type, const char* filename);
void gl_check_error();

#endif /* CONSUMER_OPENGL_UTILS */
