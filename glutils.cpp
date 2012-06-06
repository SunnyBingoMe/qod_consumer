#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "glutils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

/* Imported from libportable (c) 2006-2011 David Sveningsson */
/* modified slightly to have less deps and checks but also dropped all portability (yeah the original compiles/runs anwhere but is 200 lines) */
float getTimef(){
  static double reference = -1;

  timeval tval;
  gettimeofday(&tval, NULL);
  double tmp = (double)tval.tv_sec + ((float)(tval.tv_usec) * 1e-6f);

  if ( reference < 0 ){
    reference = tmp;
    return 0.0f;
  }

  if ( tmp - reference < 0 ){
    abort();
  }

  if ( (float)(tmp - reference) < 0 ){
    abort();
  }

  return (float)(tmp - reference);
}

/* Imported from blueflower (c) 2009 David Sveningsson */
Framerate::Framerate(float max_dt, unsigned int fps)
  : _timestamp(getTimef())
  , _fps_stamp(getTimef())
  , _max_dt(max_dt)
  , _frame_counter(0)
  , _frame_iteration(0)
  , _current_fps(0)
  , _average_fps(0.0f)
  , _timebase(0.0f) {
  
  set_fps(fps);
}
  
Framerate::~Framerate(){
    
}

void Framerate::set_fps(unsigned int n){
  if ( n > 0 ){
    _timebase = 1.0f / n;
  } else {
    _timebase = 0.0f;
  }
}

float Framerate::update(){
  float current = getTimef();
  float dt = current - _timestamp;

  if ( _timebase == 0.0f ){
    _timestamp = current;
  } else {
    _timestamp += _timebase;
  }
  _frame_counter++;

  /* stop peak dt */
  if ( dt > _max_dt ){
    printf("Warning: large dt (%f)\n", dt);
    dt = _max_dt;
  }

  if ( dt < _timebase ){
    float t = (_timebase-dt)*1000*1000;
    usleep((int)t);
  }

  /* update fps */
  if ( current - _fps_stamp > 1.0f ){
    _current_fps = _frame_counter;
    calculate_average_fps(_frame_counter);

    _frame_counter = 0;
    _fps_stamp += 1.0f;
    
    refresh_framerate();
  }

  return dt;
}

void Framerate::calculate_average_fps(unsigned int sample){
  float d = (float)sample - _average_fps;
  _average_fps = (d / (++_frame_iteration)) + _average_fps;
}

static long file_size(FILE* file){
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  return size;
}
	
static GLchar* read_file(FILE* file){
  if ( !file ){
    return NULL;
  }
  
  size_t size = file_size(file);
  GLchar* buf = (GLchar*)malloc(size+1);
  
  if ( fread(buf, 1, size, file) < size ){
    return NULL;
  }

  buf[size] = 0;
  fclose(file);
  
  return buf;
}

void print_log(GLuint shader){
  int infologLength = 0;
  char infoLog[1024];
  
  if (glIsShader(shader))
    glGetShaderInfoLog(shader, 1024, &infologLength, infoLog);
  else
    glGetProgramInfoLog(shader, 1024, &infologLength, infoLog);
  
  if (infologLength > 0){
    fprintf(stderr, "%s", infoLog);
  }
}

GLuint compile(GLuint type, const char* filename){
  FILE* fp = fopen(filename, "r");
  if ( !fp ){
    fprintf(stderr, "failed to open %s: %s\n", filename, strerror(errno));
    return 0;
  }
  
  GLchar* source = read_file(fp);

  GLuint shader = glCreateShader(type);
  glShaderSource(shader , 1, (const GLchar**)&source , NULL);
  glCompileShader(shader);
  
  GLint compile_status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  if ( compile_status == GL_FALSE){
    print_log(shader);
  }
  
  free(source);

  return shader;
}

void gl_check_error(){
  GLenum err = glGetError();
  if ( err == GL_NO_ERROR ){
    return;
  }
  
  switch(err){
  case GL_INVALID_ENUM:
  case GL_INVALID_VALUE:
  case GL_INVALID_OPERATION:
  case GL_STACK_OVERFLOW:
  case GL_STACK_UNDERFLOW:
  case GL_OUT_OF_MEMORY:
    fprintf(stderr, "OpenGL error: %s", gluErrorString(err));
    break;
    
  case GL_INVALID_FRAMEBUFFER_OPERATION:
    {
      GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      switch(status) {
      case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
	fprintf(stderr, "OpenGL FBO error: Framebuffer incomplete, incomplete attachment\n");
	break;
      case GL_FRAMEBUFFER_UNSUPPORTED:
	fprintf(stderr, "OpenGL FBO error: Unsupported framebuffer format\n");
	break;
      case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
	fprintf(stderr, "OpenGL FBO error: Framebuffer incomplete, missing attachment\n");
	break;
      case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
	fprintf(stderr, "OpenGL FBO error: Framebuffer incomplete, attached images must have same dimensions\n");
	break;
      case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
	fprintf(stderr, "OpenGL FBO error: Framebuffer incomplete, attached images must have same format\n");
	break;
      case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
	fprintf(stderr, "OpenGL FBO error: Framebuffer incomplete, missing draw buffer\n");
	break;
      case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
	fprintf(stderr, "OpenGL FBO error: Framebuffer incomplete, missing read buffer\n");
	break;
      default:
	fprintf(stderr, "OpenGL FBO error: (0x%x)\n", status);
	break;
      }
    }
  default:
    fprintf(stderr, "OpenGL error: (0x%x)\n", err);
    break;
  }
}
