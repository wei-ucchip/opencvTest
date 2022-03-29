#pragma once

#include <QtWidgets/QWidget>
#include "ui_Capillary.h"

class Capillary : public QWidget
{
    Q_OBJECT

public:
    Capillary(QWidget *parent = Q_NULLPTR);

private:
    Ui::CapillaryClass ui;
};
