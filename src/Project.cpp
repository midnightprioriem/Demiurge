//
// Created by kuhlwein on 4/9/19.
//

#include <iostream>
#include <GL/gl3w.h>
#include <vector>
#include "Project.h"
#include "Vbo.h"
#include "ShaderProgram.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Project::Project() {

    std::string code =  R"(
#version 430
layout (location = 0) in vec2 vp;
layout (location = 1) in vec2 vt;
out vec2 st;

void main () {
   st = vt;
   gl_Position = vec4 (vp, 0.0, 1.0);
}
    )";

    std::string code2 =  R"(
#version 430
in vec2 st;
uniform sampler2D img;
out vec4 fc;

void main () {
    float r = texture(img,st).r;
    fc = vec4(r,r,r,0);
}
    )";


    program = ShaderProgram::builder()
            .addShader(code,GL_VERTEX_SHADER)
            .addShader(code2,GL_FRAGMENT_SHADER)
            .link();

    int w;
    int h;
    int comp;
    std::string filename = "/home/kuhlwein/Desktop/heightdata.png";

    stbi_info(filename.c_str(),&w,&h,&comp);

    unsigned char* image = stbi_load(filename.c_str(), &w, &h, &comp, STBI_rgb);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    int format = GL_RED;//GL_RGBA;
    { // create the texture
        GLuint id;
        glGenTextures(1,&id);
        glActiveTexture( GL_TEXTURE0 );
        glBindTexture( GL_TEXTURE_2D, id );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
        // linear allows us to scale the window up retaining reasonable quality
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
        // same internal format as compute shader input

        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);

        // bind to image unit so can write to specific pixels from the shader
        glBindImageTexture( 0, id, 0, false, 0, GL_WRITE_ONLY, GL_R32F );
        glBindTexture( GL_TEXTURE_2D, id );
    }

    canvas = new Canvas(h,w);

}

void Project::update(){

}

void Project::render(){
    program->bind();
    canvas->render();
}

Project::~Project() {
    delete(program);
    delete(canvas);
    std::cout << "a\n";
}
