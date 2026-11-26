// ============================================================================
//  GLInclude.h - inclui OpenGL/GLU/freeglut de forma portavel (Linux/Windows/macOS)
// ============================================================================
#pragma once
#ifdef _WIN32
  #include <windows.h>
#endif
#if defined(__APPLE__)
  #include <GLUT/glut.h>
#elif defined(USE_GLUT_H)
  #include <GL/glut.h>       // IDEs que trazem so o GLUT classico (ex.: Falcon C++)
#else
  #include <GL/freeglut.h>   // ja inclui GL/gl.h e GL/glu.h
#endif
