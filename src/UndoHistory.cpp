//
// Created by kuhlwein on 4/3/20.
//

#include <iostream>
#include "Project.h"
#include "UndoHistory.h"



ReversibleHistory::ReversibleHistory(std::function<void(Project* p)> r, std::function<void(Project* p)> u) {
	this->r = r;
	this->u = u;
}
void ReversibleHistory::undo(Project *p) {u(p);}
void ReversibleHistory::redo(Project *p) {r(p);}


SnapshotHistory::SnapshotHistory(TextureData* data, std::function<Texture *(Project *p)> filter_target) {
	this->data = data;
	this->filter_target = filter_target;
}
void SnapshotHistory::undo(Project *p) {
	auto d = data->get();
	p->get_scratch1()->uploadData(GL_RED,GL_FLOAT,d.get());

	ShaderProgram *program = ShaderProgram::builder()
			.addShader(vertex2D->getCode(), GL_VERTEX_SHADER)
			.addShader(copy_img->getCode(), GL_FRAGMENT_SHADER)
			.link();
	p->apply(program,p->get_scratch2(),{{filter_target(p),"to_be_copied"}});

	Shader* img_diff = Shader::builder()
			.include(fragmentBase)
			.create("",R"(
fc = texture(scratch2,st).r + texture(scratch1, st).r;
)");
	ShaderProgram *program2 = ShaderProgram::builder()
			.addShader(vertex2D->getCode(), GL_VERTEX_SHADER)
			.addShader(img_diff->getCode(), GL_FRAGMENT_SHADER)
			.link();
	p->apply(program2,filter_target(p));
}
void SnapshotHistory::redo(Project *p) {
	auto d = data->get();
	p->get_scratch1()->uploadData(GL_RED,GL_FLOAT,d.get());

	ShaderProgram *program = ShaderProgram::builder()
			.addShader(vertex2D->getCode(), GL_VERTEX_SHADER)
			.addShader(copy_img->getCode(), GL_FRAGMENT_SHADER)
			.link();
	p->apply(program,p->get_scratch2(),{{filter_target(p),"to_be_copied"}});

	Shader* img_diff = Shader::builder()
			.include(fragmentBase)
			.create("",R"(
fc = texture(scratch2,st).r - texture(scratch1, st).r;
)");
	ShaderProgram *program2 = ShaderProgram::builder()
			.addShader(vertex2D->getCode(), GL_VERTEX_SHADER)
			.addShader(img_diff->getCode(), GL_FRAGMENT_SHADER)
			.link();
	p->apply(program2,filter_target(p));
}

SnapshotHistory::~SnapshotHistory() {
	delete data;
}


deleteLayerHistory::deleteLayerHistory(int id) {
	this->id = id;
}

void deleteLayerHistory::undo(Project *p) {
	p->add_layer(layer);
	p->set_layer(layer->id);
	shouldDeleteLayer = false;
}

void deleteLayerHistory::redo(Project *p) {
	shouldDeleteLayer = true;
	layer = p->get_layer(id);
	p->remove_layer(id);
}

deleteLayerHistory::~deleteLayerHistory() {
	if (shouldDeleteLayer) delete layer;
}
