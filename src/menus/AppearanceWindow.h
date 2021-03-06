//
// Created by kuhlwein on 7/14/20.
//

#ifndef DEMIURGE_APPEARANCEWINDOW_H
#define DEMIURGE_APPEARANCEWINDOW_H

#include <Menu.h>

class Project;
class Appearance;

class AppearanceWindow : public Window {
public:
	AppearanceWindow();
	bool update(Project* p) override;
	void setShader(Project* p);
	void prepare(Project* p);
private:
	std::vector<Appearance*> appearances;
	void add(Appearance* a, Project* p);
};



#endif //DEMIURGE_APPEARANCEWINDOW_H
