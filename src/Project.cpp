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
#include "Texture.h"
#include "Shader.h"

//todo remove

#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <select/AllSelect.h>
#include <select/InverseSelect.h>
#include <filter/BlurMenu.h>
#include "select/FreeSelect.h"
#include "geometry/SphericalGeometry.h"
#include "projections/Orthographic.h"
#include "projections/Mollweide.h"
#include "projections/Mercator.h"
#include "projections/Equiretangular.h"
#include "projections/img.h"
#include "menus/CanvasMenu.h"
#include "projections/Sinusoidal.h"
#include "projections/GoodeHomolosine.h"
#include "projections/EckertIV.h"
#include "menus/BrushWindow.h"
#include "menus/AppearanceWindow.h"


void Project::file_load(const std::string& filename) {
	int w, h, comp;
	stbi_info(filename.c_str(),&w,&h,&comp);
	unsigned char* image = stbi_load(filename.c_str(), &w, &h, &comp, STBI_rgb);

	file_new(w,h);

	get_terrain()->uploadData(GL_RGB,GL_UNSIGNED_BYTE,image);
}

void Project::file_new(int w, int h) {
	width=w;
	height = h;
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	remove_texture(terrain);
	delete terrain;
	terrain = new Texture(w,h,GL_R32F,"img");
	remove_texture(scratchPad2);
	delete scratchPad2;
	scratchPad2 = new Texture(w,h,GL_R32F,"scratch2");
	remove_texture(scratchPad);
	delete scratchPad;
	scratchPad = new Texture(w,h,GL_R32F,"scratch1");
	remove_texture(selection);
	delete selection;
	selection = new Texture(w,h,GL_R32F,"sel");

	for (auto t : {terrain,scratchPad, scratchPad2, selection}) add_texture(t);
	layers = {{"Default",terrain}};
	current_layer = 0;

	ShaderProgram *program = ShaderProgram::builder()
			.addShader(vertex2D->getCode(), GL_VERTEX_SHADER)
			.addShader(fragment_set->getCode(), GL_FRAGMENT_SHADER)
			.link();
	apply(program,get_terrain());
	int id = glGetUniformLocation(program->getId(),"value");
	glUniform1f(id,1.0f);
	apply(program,get_selection());

	while(!undo_list.empty()) undo_list.pop();
	while(!redo_list.empty()) redo_list.pop();

	canvas = new img(this);
	update_terrain_shader();

}

Project::Project(GLFWwindow* window) {
    ImGui::GetIO().IniFilename = NULL;

    this->window = window;

	//Used for rendering to external framebuffer
    glGenFramebuffers(1,&fbo);

    std::vector<float> positions = {
            -1, -1, 0,
            -1, 1, 0,
            1, 1, 0,
            1, -1, 0
    };
    std::vector<float> textures = {
            0.0, 0.0,
            0.0, 1,
            1, 1,
            1, 0
    };
    std::vector<int> indices = {
            0, 1, 3,
            2, 3, 1
    };
    vbo = new Vbo(positions, textures, indices);

	std::vector<Menu*> file_menu = {};
	file_menu.push_back(new Modal("New...", testnamespace::file_new));
	file_menu.push_back(new Modal("Load...", testnamespace::file_load));

	std::vector<Menu*> edit_menu = {};
	edit_menu.push_back(new Menu("Undo",edit::undo));
	edit_menu.push_back(new Menu("Redo",edit::redo));
	edit_menu.push_back(new SeparatorMenu());
	edit_menu.push_back(new Modal("Preferences",edit::preferences));

	std::vector<Menu*> projections = {};
	projections.push_back(new Menu("None", [](Project* p) {
		p->canvas = new img(p);
		p->update_terrain_shader();
		return true;
	}));
	projections.push_back(new CanvasMenu("Equiretangular...",new Equiretangular(this)));
	projections.push_back(new Menu("Orthographic", [](Project* p){
		p->canvas = new Orthographic(p);
		p->update_terrain_shader();
		return true;
	}));

	projections.push_back(new CanvasMenu("Mollweide...",new Mollweide(this)));
	projections.push_back(new CanvasMenu("Sinusoidal...",new Sinusoidal(this)));
	projections.push_back(new CanvasMenu("Goode Homolosine...",new GoodeHomolosine(this)));
	projections.push_back(new CanvasMenu("Eckert IV...",new EckertIV(this)));
	projections.push_back(new CanvasMenu("Mercator...",new Mercator(this)));

	std::vector<Menu*> windows_menu = {};
	windows_menu.push_back(new BrushWindow("Brush",this));
	appearanceWindow = new AppearanceWindow("Appearance");
	windows_menu.push_back(appearanceWindow);
	windows_menu.push_back(new Window("Layers", testnamespace::layers));


	std::vector<Menu*> selection_menu = {};

	selection_menu.push_back(new AllSelect());
	selection_menu.push_back(new InverseSelect());
	selection_menu.push_back(new FreeSelect());

	std::vector<Menu*> filter_menu = {};
	filter_menu.push_back(new BlurMenu());

	windows.emplace_back("File",file_menu);
	windows.emplace_back("Edit",edit_menu);
	windows.emplace_back("Select",selection_menu);
	windows.emplace_back("Projections",projections);
	windows.emplace_back("Filter",filter_menu);
	windows.emplace_back("Windows",windows_menu);

	filter = std::make_unique<NoneFilter>();

	geometry = new SphericalGeometry(this);

	canvas = new img(this);
	appearanceWindow->setShader(this);



	file_new(1000,500);
}

void Project::update() {
    /*
     File
     Edit
     Select
     View
     Render
     filters
    */

    if(ImGui::BeginMainMenuBar()) {
		for(auto p : windows) {
			if (ImGui::BeginMenu(p.first.c_str(),true)) {
				for (Menu* w : p.second) w->menu();
				ImGui::EndMenu();
			}
		}
        ImGui::EndMainMenuBar();
    }


	for(auto p : windows) {
		for (Menu* w : p.second) w->update(this);
	}

	//TODO something
	//filter->run();

    canvas->update();

	ImGuiIO io = ImGui::GetIO();

	int z_key = 47;
	if (io.KeysDown[47] && io.KeyCtrl && io.KeysDownDuration[z_key] == 0.0f && io.KeyShift) {
		redo();
	} else if (io.KeysDown[47] && io.KeyCtrl && io.KeysDownDuration[z_key] == 0.0f) {
		undo();
	}


//	static bool test = true;
//
//	static GLsync sync;
//	static GLuint pbo;
//
//	if (test = true) {
//		glGenBuffers(1, &pbo);
//		glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
//		test = false;
//	}
//	if (io.KeysDown[47] && io.KeysDownDuration[47]==0.0f) {
//
//		glBufferData(GL_PIXEL_PACK_BUFFER, getWidth() * getHeight() * 4, NULL, GL_STREAM_READ);
//		std::cout << "pressed\n";
//
//		glActiveTexture(GL_TEXTURE0);
//		terrain->bind(0);
//		//glBindTexture(GL_TEXTURE_2D, texture);
//
//		glGetTexImage(GL_TEXTURE_2D,
//					  0,
//					  GL_RGBA,
//					  GL_UNSIGNED_BYTE,
//					  nullptr);
////ensure we don't try and read data before the transfer is complete
//		sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
//
//	}
//
//
//// then regularly check for completion
//	GLint result;
//	glGetSynciv(sync, GL_SYNC_STATUS, sizeof(result), NULL, &result);
//
//	if(result == GL_SIGNALED){
//		glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
//		void* mappedBuffer = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
//
//		//now mapped buffer contains the pixel data
//
//		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
//
//	} else {
//		std::cout << "not finished\n";
//	}



}

void Project::render() {
    program->bind();
    bind_textures(program);

    canvas->render();
}

Project::~Project() {
    delete(program);
    delete(canvas);
}

int Project::getWindowWidth() {
    int width, height;
    glfwGetWindowSize(window,&width,&height);
    return width;
}

int Project::getWindowHeight() {
    int width, height;
    glfwGetWindowSize(window,&width,&height);
    return height;
}

void Project::apply(ShaderProgram *program, Texture *texture, std::vector<std::pair<Texture*,std::string>> l) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, texture->getWidth(), texture->getHeight());
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture->getId(),0);
    program->bind();
    bind_textures(program, l);

    vbo->render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Project::bind_textures(ShaderProgram *program,std::vector<std::pair<Texture*,std::string>> l) {
	int i=0;
	for (auto t : textures) t->bind(program,i++);
	for (auto p : l) p.first->bind(program,i++,p.second);
}

void Project::add_texture(Texture *texture) {
	textures.insert(texture);
}

void Project::remove_texture(Texture *texture) {
	textures.erase(texture);
}

Texture *Project::get_terrain() {
	return terrain;
}

void Project::set_terrain(Texture *texture) {
	remove_texture(terrain);
	terrain = texture;
	add_texture(texture);
}

int Project::getWidth() {
	return width;
}

int Project::getHeight() {
	return height;
}

int Project::get_n_layers() {
	return layers.size();
}

void Project::add_layer(std::pair<std::string,Texture*> l,int index) {
	if (index>0) layers.insert(layers.begin()+index,l);
	else layers.push_back(l);
}

std::pair<std::string,Texture*> Project::get_layer(int i) {
	return layers[i];
}

int Project::get_current_layer() {
	return current_layer;
}

void Project::set_layer(int i) {
	current_layer = i;
	set_terrain(layers[i].second);
}

void Project::remove_layer(int i) {
	if (i>0) {
		layers.erase(layers.begin()+i);
		if (get_current_layer()>get_n_layers()-1) set_layer(get_n_layers()-1);
	}
}

void Project::update_terrain_shader() {

	Shader::builder builder = Shader::builder()
			.include(fragmentColor)
			.include(geometry->distance())
			.include(canvas->projection_shader())
			.include(terrain_shader);

			builder.include(filter->getShader());

			Shader* shader = builder
			.include(brush_outline)
			.include(selection_outline)
			.create();

	program = ShaderProgram::builder()
			.addShader(vertex3D->getCode(),GL_VERTEX_SHADER)
			.addShader(shader->getCode(),GL_FRAGMENT_SHADER)
			.link();

	appearanceWindow->prepare(this);
}

Texture *Project::get_selection() {
	return selection;
}

void Project::add_history(UndoHistory* h) {
	undo_list.push(h);
	while(!redo_list.empty()) {
		redo_list.pop();
	}
}

void Project::undo() {
	if (undo_list.empty()) return;
	UndoHistory* h = undo_list.top();
	undo_list.pop();
	h->undo(this);
	redo_list.push(h);

}

void Project::redo() {
	if (redo_list.empty()) return;
	UndoHistory* h = redo_list.top();
	redo_list.pop();
	h->redo(this);
	undo_list.push(h);
}

int Project::get_n_textures() {
	return textures.size();
}

Texture *Project::get_scratch2() {
	return scratchPad2;
}

Texture *Project::get_scratch1() {
	return scratchPad;
}

void Project::set_terrain_shader(Shader *s) {
	terrain_shader = s;
	update_terrain_shader();
}

void Project::setCoords(std::vector<float> v) {
	coords = v;
}

std::vector<float> Project::getCoords() {
	return coords;
}

glm::vec2 Project::getMouse() {
	ImGuiIO io = ImGui::GetIO();
	return canvas->mousePos(io.MousePos);
}

glm::vec2 Project::getMousePrev() {
	ImGuiIO io = ImGui::GetIO();
	return canvas->mousePos(ImVec2(io.MousePos.x - io.MouseDelta.x,io.MousePos.y - io.MouseDelta.y));
}

void Project::dispatchFilter(std::shared_ptr<Filter> filter) {
	this->filter = std::move(filter);
	update_terrain_shader();
}

void Project::finalizeFilter() {
	filter = std::make_unique<NoneFilter>();
	update_terrain_shader();
}

void Project::setGeometry(Geometry *g) {
	geometry = g;
	update_terrain_shader();
}

Geometry *Project::getGeometry() {
	return geometry;
}


