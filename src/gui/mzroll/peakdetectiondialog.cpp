#include <string>

#include <QVariant>

#include "alignmentdialog.h"
#include "analytics.h"
#include "background_peaks_update.h"
#include "classifierNeuralNet.h"
#include "database.h"
#include "ligandwidget.h"
#include "mainwindow.h"
#include "masscalcgui.h"
#include "mavenparameters.h"
#include "mzSample.h"
#include "peakdetectiondialog.h"
#include "PeakDetector.h"
#include "settingsform.h"
#include "tabledockwidget.h"
#include "videoplayer.h"

PeakDetectionSettings::PeakDetectionSettings(PeakDetectionDialog* dialog):pd(dialog)
{
    // automated feature detection settings
    settings.insert("automatedDetection", QVariant::fromValue(pd->featureOptions));
    settings.insert("massDomainResolution", QVariant::fromValue(pd->ppmStep));
    settings.insert("timeDomainResolution", QVariant::fromValue(pd->rtStep));
    settings.insert("minMz", QVariant::fromValue(pd->mzMin));
    settings.insert("maxMz", QVariant::fromValue(pd->mzMax));
    settings.insert("minRt", QVariant::fromValue(pd->rtMin));
    settings.insert("maxRt", QVariant::fromValue(pd->rtMax));
    settings.insert("minIntensity", QVariant::fromValue(pd->minIntensity));
    settings.insert("maxIntensity", QVariant::fromValue(pd->maxIntensity));
    settings.insert("chargeMax", QVariant::fromValue(pd->chargeMax));
    settings.insert("chargeMin", QVariant::fromValue(pd->chargeMin));
    settings.insert("mustHaveFragmentation", QVariant::fromValue(pd->mustHaveMs2));

    // db search settings
    settings.insert("databaseSearch", QVariant::fromValue(pd->dbOptions));
    settings.insert("compoundExtractionWindow", QVariant::fromValue(pd->compoundPPMWindow));
    settings.insert("matchRt", QVariant::fromValue(pd->matchRt));
    settings.insert("compoundRtWindow", QVariant::fromValue(pd->compoundRTWindow));
    settings.insert("limitGroupsPerCompound", QVariant::fromValue(pd->eicMaxGroups));

    // fragmentation settings
    settings.insert("matchFragmentation", QVariant::fromValue(pd->matchFragmentationOptions));
    settings.insert("minFragMatchScore", QVariant::fromValue(pd->minFragMatchScore));
    settings.insert("fragmentTolerance", QVariant::fromValue(pd->fragmentTolerance));
    settings.insert("minFragMatch", QVariant::fromValue(pd->minFragMatch));

    // isotope detection
    settings.insert("reportIsotopes", QVariant::fromValue(pd->reportIsotopesOptions));

    // group filtering settings
    settings.insert("peakQuantitation", QVariant::fromValue(pd->peakQuantitation));
    settings.insert("minGroupIntensity", QVariant::fromValue(pd->minGroupIntensity));
    settings.insert("intensityQuantile", QVariant::fromValue(pd->quantileIntensity));
    settings.insert("minGroupQuality", QVariant::fromValue(pd->doubleSpinBoxMinQuality));
    settings.insert("qualityQuantile", QVariant::fromValue(pd->quantileQuality));
    settings.insert("minSignalBlankRatio", QVariant::fromValue(pd->sigBlankRatio));
    settings.insert("signalBlankRatioQuantile", QVariant::fromValue(pd->quantileSignalBlankRatio));
    settings.insert("minSignalBaselineRatio", QVariant::fromValue(pd->sigBaselineRatio));
    settings.insert("signalBaselineRatioQuantile", QVariant::fromValue(pd->quantileSignalBaselineRatio));
    settings.insert("minPeakWidth", QVariant::fromValue(pd->minNoNoiseObs));
    settings.insert("minGoodPeakCount", QVariant::fromValue(pd->minGoodGroupCount));
    settings.insert("peakClassifierFile", QVariant::fromValue(pd->classificationModelFilename));

    /* special case: there is no Ui element defined inside Peaks dialog that can be used
     * to change/access massCutOfftype. the only way to change massCutofftype is to change it from mainWindow(top right corner).
     * PeakDetectionDialog::masCutOffType is a variable that stores the value of MassCutOfftype defined in mainWindow
     * Better would be to have a ui element that allows to change/access massCutoff from peaks dialog
     */
    settings.insert("massCutoffType", QVariant::fromValue(&pd->massCutoffType));
}

void PeakDetectionSettings::updatePeakSettings(string key, string value)
{
    QString k(QString(key.c_str()));
    if (settings.find(k) != settings.end()
        && !value.empty()) {
        const QVariant& v = settings[k];
        // convert the val to proper type;
        if(QString(v.typeName()).contains("QDoubleSpinBox"))
            v.value<QDoubleSpinBox*>()->setValue(std::stod(value));

        if(QString(v.typeName()).contains("QGroupBox"))
            v.value<QGroupBox*>()->setChecked(std::stod(value));

        if(QString(v.typeName()).contains("QCheckBox"))
            v.value<QCheckBox*>()->setChecked(std::stod(value));

        if(QString(v.typeName()).contains("QSpinBox"))
            v.value<QSpinBox*>()->setValue(std::stod(value));

        if(QString(v.typeName()).contains("QSlider"))
            v.value<QSlider*>()->setValue(std::stod(value));

        if(QString(v.typeName()).contains("QComboBox"))
            v.value<QComboBox*>()->setCurrentIndex(std::stoi(value));

        if(QString(v.typeName()).contains("QLineEdit"))
            v.value<QLineEdit*>()->setText(QString(value.c_str()));

        /* IMPORTANT
         * special case: only pd->massCutOfftype  and the places where it is used are updated here
         * there is no other Ui element that with  typeName as "QString".
         * Better solution is to have a Ui element in Peaks Dialog that can be used to
         * change/access massCutoff type
         */
        if(QString(v.typeName()).contains("QString")) {
            pd->massCutoffType = value.c_str();
            pd->getMainWindow()->massCutoffComboBox->setCurrentText(pd->massCutoffType);
        }

        emit pd->settingsUpdated(k, v);
    }
}

PeakDetectionDialog::PeakDetectionDialog(MainWindow* parent) :
        QDialog(parent)
{
        setupUi(this);

        settings = NULL;
        mainwindow = parent;

        setModal(false);
        peakupdater = NULL;

        massCutoffType = "ppm";
        peakSettings = new PeakDetectionSettings(this);

        peakupdater = new BackgroundPeakUpdate(this);
        if (mainwindow) peakupdater->setMainWindow(mainwindow);


        connect(resetButton, &QPushButton::clicked, this, &PeakDetectionDialog::onReset);
        connect(compoundDatabase, SIGNAL(currentTextChanged(QString)), SLOT(toggleFragmentation(QString)));
        connect(computeButton, SIGNAL(clicked(bool)), SLOT(findPeaks()));
        connect(cancelButton, SIGNAL(clicked(bool)), SLOT(cancel()));
        connect(setOutputDirButton, SIGNAL(clicked(bool)), SLOT(setOutputDir()));
        connect(matchRt,SIGNAL(clicked(bool)),compoundRTWindow,SLOT(setEnabled(bool))); //TODO: Sahil - Kiran, Added while merging mainwindow
        connect(tabwidget,SIGNAL(currentChanged(int)),this,SLOT(showMethodSummary())); //TODO: Sahil - Kiran, Added while merging mainwindow
        connect(tabwidget,SIGNAL(currentChanged(int)),this,SLOT(updatePeakTableList())); //TODO: Sahil - Kiran, Added while merging mainwindow
        connect(reportIsotopesOptions,SIGNAL(clicked(bool)),this,SLOT(showMethodSummary()));
        connect(reportIsotopesOptions,
                &QGroupBox::toggled,
                [this](const bool checked)
                {
                    QString state = checked? "On" : "Off";
                    this->mainwindow
                        ->getAnalytics()
                        ->hitEvent("Peak Detection",
                                   "Isotope Detection Switched",
                                   state);
                });
        connect(matchFragmentationOptions,
                &QGroupBox::toggled,
                [this](const bool checked)
                {
                    QString state = checked? "On" : "Off";
                    this->mainwindow
                        ->getAnalytics()
                        ->hitEvent("Peak Detection",
                                   "Match Fragmentation Switched",
                                   state);
                    if (state == "On") {
                        this->mainwindow
                            ->getAnalytics()
                            ->hitEvent("PRM", "PRM Analysis");
                    }
                });
        connect(fragmentTolerance,
                SIGNAL(valueChanged(double)),
                mainwindow->massCalcWidget->fragPpm,
                SLOT(setValue(double)));
        connect(saveMethodButton,SIGNAL(clicked()),this,SLOT(saveMethod())); //TODO: Sahil - Kiran, Added while merging mainwindow
        connect(loadMethodButton,SIGNAL(clicked()),this,SLOT(loadMethod())); //TODO: Sahil - Kiran, Added while merging mainwindow
        connect(loadModelButton,SIGNAL(clicked()),this,SLOT(loadModel()));

        connect(quantileIntensity,SIGNAL(valueChanged(int)),this, SLOT(showIntensityQuantileStatus(int)));
        connect(quantileQuality, SIGNAL(valueChanged(int)), this, SLOT(showQualityQuantileStatus(int)));
        connect(quantileSignalBaselineRatio, SIGNAL(valueChanged(int)), this, SLOT(showBaselineQuantileStatus(int)));
        connect(quantileSignalBlankRatio, SIGNAL(valueChanged(int)), this, SLOT(showBlankQuantileStatus(int)));

        connect(this, &QDialog::rejected, this, &PeakDetectionDialog::triggerSettingsUpdate);

        label_20->setVisible(false);
        chargeMin->setVisible(false);
        chargeMax->setVisible(false);
        
        connect(dbOptions, SIGNAL(toggled(bool)), SLOT(dbOptionsClicked()));
        featureOptions->setChecked(false);
        connect(featureOptions, SIGNAL(toggled(bool)), SLOT(featureOptionsClicked()));

        compoundRTWindow->setEnabled(false); //TODO: Sahil - Kiran, Added while merging mainwindow
        reportIsotopesOptions->setEnabled(true); //TODO: Sahil - Kiran, Added while merging mainwindow
        //_featureDetectionType = CompoundDB; //TODO: Sahil - Kiran, removed while merging mainwindow
        connect(changeIsotopeOptions,SIGNAL(clicked()),this, SLOT(showSettingsForm()));

        connect(classificationModelFilename,
                SIGNAL(textChanged(QString)),
                this,
                SLOT(setModel(QString)));

        connect(this, &PeakDetectionDialog::settingsChanged, peakSettings, &PeakDetectionSettings::updatePeakSettings);

}

void PeakDetectionDialog::onReset()
{
    emit resetSettings(peakSettings->getSettings().keys());
}

void PeakDetectionDialog::setMassCutoffType(QString type)
{
    massCutoffType = type;
    auto suffix = QString(" %1").arg(type);
    ppmStep->setSuffix(suffix);
    compoundPPMWindow->setSuffix(suffix);
    emit updateSettings(peakSettings);
}

void PeakDetectionDialog::closeEvent(QCloseEvent* event)
{
    // update maven peak settings whenever we close the dilaog box or click on 'cancel' button. 
    // cancel in turn calls close();
    emit updateSettings(peakSettings);
}

void PeakDetectionDialog::showSettingsForm() {

    mainwindow->getAnalytics()->hitScreenView("OptionsDialog");    
    mainwindow->settingsForm->exec();
    mainwindow->settingsForm->setIsotopeDetectionTab();
}

void PeakDetectionDialog::dbOptionsClicked()
{   
    if (dbOptions->isChecked()) {
        mainwindow->alignmentDialog->peakDetectionAlgo->setCurrentIndex(0);
        featureOptions->setChecked(false);
        reportIsotopesOptions->setEnabled(true);
    } else {
        mainwindow->alignmentDialog->peakDetectionAlgo->setCurrentIndex(1);
        featureOptions->setChecked(true);
        reportIsotopesOptions->setEnabled(false);
    }
    
    QString dbName = compoundDatabase->currentText();
    toggleFragmentation(dbName);
}

void PeakDetectionDialog::triggerSettingsUpdate()
{
    // happens when users presses 'esc' key; 
    emit updateSettings(peakSettings);
}

void PeakDetectionDialog::featureOptionsClicked()
{
    if (featureOptions->isChecked()) {
        mainwindow->alignmentDialog->peakDetectionAlgo->setCurrentIndex(1);
        dbOptions->setChecked(false);
        reportIsotopesOptions->setEnabled(false);
    } else {
        mainwindow->alignmentDialog->peakDetectionAlgo->setCurrentIndex(0);
        dbOptions->setChecked(true);
        reportIsotopesOptions->setEnabled(true);
    }

    QString dbName = compoundDatabase->currentText();
    toggleFragmentation(dbName);
}

PeakDetectionDialog::~PeakDetectionDialog() {
    if (peakupdater) delete (peakupdater);
}
/**
 * PeakDetectionDialog::cancel Stoping the peak detection process
 */
void PeakDetectionDialog::cancel() {
    
    if (peakupdater) {
        if (peakupdater->isRunning()) {
            peakupdater->completeStop();
            return;
        }
    }
    close();
}

void PeakDetectionDialog::initPeakDetectionDialogWindow(
    FeatureDetectionType type) {
    displayAppropriatePeakDetectionDialog(type);
    inputInitialValuesPeakDetectionDialog();
}
/**
 * PeakDetectionDialog::setFeatureDetection This function is being called when
 * the database search button or the feature detection button is cliecked
 * from the main window
 * @param type this is an enum which can take the following values
 * FullSpectrum, CompoundDB, QQQ
 */
void PeakDetectionDialog::displayAppropriatePeakDetectionDialog(
    FeatureDetectionType type) {
    _featureDetectionType = type;

    if (_featureDetectionType == QQQ) {
        dbOptions->hide();
        featureOptions->hide();
    } else if (_featureDetectionType == FullSpectrum) {
        dbOptions->hide();
        featureOptions->show();
    } else if (_featureDetectionType == CompoundDB) {
        dbOptions->show();
        featureOptions->hide();
    }

    tabwidget->setCurrentIndex(
        0);
    adjustSize();
}

void PeakDetectionDialog::show() {

    if (mainwindow == NULL) return;

    QString dbName = compoundDatabase->currentText();
    toggleFragmentation(dbName);

	mainwindow->getAnalytics()->hitScreenView("PeakDetectionDialog");
    // delete(peakupdater);
    peakupdater = new BackgroundPeakUpdate(this);
    if (mainwindow) peakupdater->setMainWindow(mainwindow);

    connect(peakupdater, SIGNAL(updateProgressBar(QString,int,int)),
               SLOT(setProgressBar(QString, int,int)));
    connect(peakupdater, SIGNAL(updateProgressBar(QString,int,int)),
               mainwindow->alignmentDialog, SLOT(setProgressBar(QString, int,int)));

    // peakupdater->useMainWindowLabelOptions = false;

    inputInitialValuesPeakDetectionDialog();

}

/**
 * PeakDetectionDialog::loadModel This function works in the peakdector window
 * When theuser clicks on the Peak classifier Model file a dialog box appears
 * from which they can seect the model that is been trained
 */
void PeakDetectionDialog::loadModel() {
    
    // This gives the name of the file that is selected by the user
    const QString modelPath =
        QFileDialog::getOpenFileName(this,
                                     "Select Classification Model",
                                     ".",
                                     tr("Model File (*.model)"));
    classificationModelFilename->setText(modelPath);
}

void PeakDetectionDialog::setModel(const QString& modelPath)
{
    // Getting the classifier instance from the main window
    Classifier* clsf = mainwindow->getClassifier();

    // Loading the model to the to the model instance
    if (clsf)
        clsf->loadModel(modelPath.toStdString());
}

/*
@author:Sahil-Kiran
*/
// TODO: Sahil - Kiran, Added while merging mainwindow
void PeakDetectionDialog::loadMethod() {
    
    const QString filename = QFileDialog::getOpenFileName(
        this, "Load Settings", ".", tr("Settings File (*.method)"));
    peakupdater->loadSettings(filename);
    inputInitialValuesPeakDetectionDialog();
}

/*
@author:Sahil-Kiran
*/
// TODO: Sahil - Kiran, Added while merging mainwindow
void PeakDetectionDialog::saveMethod() {
    
    const QString filename = QFileDialog::getSaveFileName(
        this, "Save Settings", ".", tr("Settings File (*.method)"));
    peakupdater->saveSettings(filename);
}

/**
 * PeakDetectionDialog::setOutputDir this function sets the directory of the
 * file to which the output has to be connected
 */
void PeakDetectionDialog::setOutputDir() {
    // Getting the user selected folder name and path
    const QString dirName = QFileDialog::getExistingDirectory(
        this, "Save Reports to a Folder", ".", QFileDialog::ShowDirsOnly);
    // Showing it in the output directory box
    outputDirName->setText(dirName);
}

/**
 * PeakDetectionDialog::show This function is being called when the
 * database search button or the feature detection button is cliecked
 * from the main window
 */
void PeakDetectionDialog::inputInitialValuesPeakDetectionDialog() {
    // TODO: Why only this two variables are updated in the windows that is
    // selected by the user
    if (mainwindow != NULL) {
        QSettings* settings = mainwindow->getSettings();
        if (settings) {

            // Peak Scoring and Filtering
            showQualityQuantileStatus(quantileQuality->value());
            showBaselineQuantileStatus(quantileSignalBaselineRatio->value());
            showBlankQuantileStatus(quantileSignalBlankRatio->value());
            showIntensityQuantileStatus(quantileIntensity->value());

            classificationModelFilename->setText(settings->value("peakClassifierFile").toString());

            // Isotope detection in peakdetection dialogue box
            // checkBox->setChecked(settings->value("checkBox").toBool());  // C13
            // checkBox_2->setChecked(
            //     settings->value("checkBox_2").toBool());  // N15
            // checkBox_3->setChecked(
            //     settings->value("checkBox_3").toBool());  // D2
            // checkBox_4->setChecked(
            //     settings->value("checkBox_4").toBool());  // S34

        }
        /**
         * Getting the database present and updating in the dropdown of the
         * peak detection windows
         */
        map<string, int>::iterator itr;
        map<string, int> dbnames = DB.getDatabaseNames();

        // Clearing so that old value is not appended with the new values
        compoundDatabase->clear();
        for (itr = dbnames.begin(); itr != dbnames.end(); itr++) {
            string db = (*itr).first;
            if (!db.empty()) compoundDatabase->addItem(QString(db.c_str()));
        }

        // selecting the compound database that is selected by the user in the
        // ligand widget
        QString selectedDB = mainwindow->ligandWidget->getDatabaseName();
        compoundDatabase->setCurrentIndex(
            compoundDatabase->findText(selectedDB));
        
        //match fragmentation only enabled during targeted detection with NIST library
        toggleFragmentation(selectedDB);

        QDialog::exec();
    }
}

void PeakDetectionDialog::toggleFragmentation(QString selectedDbName)
{
    if (dbOptions->isChecked() && DB.isNISTLibrary(selectedDbName.toStdString())) {
        matchFragmentationOptions->setEnabled(true);
    } else {
        matchFragmentationOptions->setChecked(false);
        matchFragmentationOptions->setEnabled(false);
    }

    auto samples = mainwindow->getVisibleSamples();
    auto iter = find_if(begin(samples),
                        end(samples),
                        [](mzSample* s) {
                           return ((s->ms1ScanCount() > 0)
                                   && (s->ms2ScanCount() > 0));
                        });
    bool foundDda = iter != end(samples);
    if (foundDda && featureOptions->isChecked()) {
        mustHaveMs2->setEnabled(true);
    } else {
        mustHaveMs2->setEnabled(false);
        mustHaveMs2->setChecked(false);
    }
}

/**
 * PeakDetectionDialog::findPeaks This is the function which excuite functions
 * related to peak detection
 * Peak detection happens in a different thread called "peakupdater"
 * According to the settings that is selected by the user, peak detection can
 * happen using different algorithm
 *
 * In this funtion all the paremeters that are input by the user is taken into
 * the maven parameter class
 *
 * Then the needed algorithm is ran
 */

// TODO: Sahil. Refactored this whole function. Merged with mainwindow of 776.
// RECHECK IT AGAIN. IMPORTANT
void PeakDetectionDialog::findPeaks()
{
    if (reportIsotopesOptions->isChecked()) {
        mainwindow->getAnalytics()->hitEvent("Peak Detection",
                                             "Find Peaks With Isotopes");
    } else {
        mainwindow->getAnalytics()->hitEvent("Peak Detection",
                                             "Find Peaks");
    }

    // IMPORTANT: we have to make sure that maven parameters are updated before we start finding peaks.
    // there are not a lot of settings that need to be updated,hence it's not late to update them right now.
    emit updateSettings(peakSettings);
    
    if (mainwindow == NULL) return;
    if (peakupdater == NULL) return;
    if (peakupdater->isRunning()) cancel();
    if (peakupdater->isRunning()) return;
    peakupdater->setUntargetedMustHaveMs2(false);

    updateQSettingsWithUserInput(settings);
    setMavenParameters(settings);

    mainwindow->setTotalCharge();

    if (dbOptions->isChecked() && !(featureOptions->isChecked())) {
        _featureDetectionType = CompoundDB;
        mainwindow->getAnalytics()->hitEvent("Peak Detection", "Targeted");
        mainwindow->massCutoffWindowBox->setValue(compoundPPMWindow->value());
    } else if (!(dbOptions->isChecked()) && (featureOptions->isChecked())) {
        _featureDetectionType = FullSpectrum;
        mainwindow->massCutoffWindowBox->setValue(ppmStep->value());
        if (mustHaveMs2->isChecked()) {
            peakupdater->setUntargetedMustHaveMs2(true);
            mainwindow->getAnalytics()->hitEvent("Peak Detection",
                                                 "Untargeted",
                                                 "Filter for MS2 events");
        } else {
            mainwindow->getAnalytics()->hitEvent("Peak Detection",
                                                 "Untargeted"
                                                 "No filter");
        }
    } else {
        _featureDetectionType = FullSpectrum;
    }

    TableDockWidget* peaksTable = mainwindow->getBookmarkedPeaks();
    int peakTableIdx = outputTableComboBox->currentIndex();

    if (peakTableIdx == 0) {
        peaksTable = mainwindow->addPeaksTable(mainwindow->lastPeakTableId);
    } else if (peakTableIdx == 1) {
        peaksTable = mainwindow->getBookmarkedPeaks();
    } else if (peakTableIdx >= 2) {
        QList<QPointer<TableDockWidget> > peaksTableList =
            mainwindow->getPeakTableList();
        if (peaksTableList.size() >= 1 and
            peakTableIdx - 2 < peaksTableList.size()) {
            peaksTable = peaksTableList[peakTableIdx - 2];
        }
    }


    // disconnect prvevious connections
    disconnect(peakupdater, SIGNAL(newPeakGroup(PeakGroup*)), 0, 0);
    disconnect(peakupdater, SIGNAL(finished()), 0, 0);
    // disconnect(peakupdater, SIGNAL(terminated()), 0, 0);

    // connect new connections
    connect(peakupdater, SIGNAL(newPeakGroup(PeakGroup*)), peaksTable,
           SLOT(addPeakGroup(PeakGroup*)));
    connect(peakupdater, SIGNAL(finished()), peaksTable, SLOT(showAllGroups()));
    // connect(peakupdater, SIGNAL(terminated()), peaksTable,
            // SLOT(showAllGroups()));
    connect(peakupdater, SIGNAL(finished()), this, SLOT(close()));
    if(!settings->value("hideVideoPlayer", 0).toBool())
        connect(peakupdater, SIGNAL(finished()), mainwindow->vidPlayer, SLOT(show()));
    // connect(peakupdater, SIGNAL(terminated()), this, SLOT(close()));
    peakupdater->setPeakDetector(new PeakDetector(peakupdater->mavenParameters));

    // RUN THREAD
    if (_featureDetectionType == FullSpectrum)
        runBackgroupJob("processMassSlices");
    else
        runBackgroupJob("computePeaks");

}

void PeakDetectionDialog::showIntensityQuantileStatus(int value) {
    mainwindow->mavenParameters->quantileIntensity= value;
    QString qstat;
    if (value) {
        std::string stat(std::to_string(value) + "% peaks above minimum intensity");
        qstat = QString::fromStdString(stat);
    }
    else {
        std::string stat("1 peak above minimum intensity");
        qstat = QString::fromStdString(stat);
    }
    intensityQuantileStatus->setText(qstat);
}

void PeakDetectionDialog::showQualityQuantileStatus(int value) {
    mainwindow->mavenParameters->quantileQuality= value;
    QString qstat;
    if (value) {
        std::string stat(std::to_string(value) + "% peaks above minimum quality");
        qstat = QString::fromStdString(stat);
    }
    else {
        std::string stat("1 peak above minimum quality");
        qstat = QString::fromStdString(stat);
    }
    qualityQuantileStatus->setText(qstat);
}

void PeakDetectionDialog::showBaselineQuantileStatus(int value) {
    mainwindow->mavenParameters->quantileSignalBaselineRatio= value;
    QString qstat;
    if (value) {
        std::string stat(std::to_string(value) + "% peaks above minimum signal/baseline ratio");        
        qstat = QString::fromStdString(stat);
    }
    else {
        std::string stat("1 peak above minimum signal/baseline ratio");
        qstat = QString::fromStdString(stat);
    }
    baselineQuantileStatus->setText(qstat);
}

void PeakDetectionDialog::showBlankQuantileStatus(int value) {
    mainwindow->mavenParameters->quantileSignalBlankRatio= value;
    QString qstat;
    if (value) {
        std::string stat(std::to_string(value) + "% peaks above minimum signal/blank ratio");
        qstat = QString::fromStdString(stat);
    }
    else {
        std::string stat("1 peak above minimum signal/blank ratio");
        qstat = QString::fromStdString(stat);
    }
    blankQuantileStatus->setText(qstat);
}

void PeakDetectionDialog::updateQSettingsWithUserInput(QSettings* settings) {
    // Peak Scoring and Filtering
    // Compound DB search
    // Automated Peak Detection
    // Isotope detection in peakdetection dialogue box
    // settings->setValue("checkBox", checkBox->isChecked());      // C13
    // settings->setValue("checkBox_2", checkBox_2->isChecked());  // N15
    // settings->setValue("checkBox_3", checkBox_3->isChecked());  // D2
    // settings->setValue("checkBox_4", checkBox_4->isChecked());  // S34

    // Enabling feature detection or compound search
    ////////////////////////////////////////////////////////////
    // TODO: what is this?
    vector<mzSample*> samples = mainwindow->getSamples();
    //TODO Sabu: This has to be taken care in a better way 
    if (samples.size() > 0) {
        settings->setValue("avgScanTime", 
                samples[0]->getAverageFullScanTime());
    }
    // Time domain resolution(scans)
}

void PeakDetectionDialog::setMavenParameters(QSettings* settings) {
    if (peakupdater->isRunning()) return;
    MavenParameters* mavenParameters = mainwindow->mavenParameters;
    if (settings != NULL) {

        //Pointing the output directory
        if (!outputDirName->text().isEmpty()) {
            mavenParameters->setOutputDir(outputDirName->text());
            mavenParameters->writeCSVFlag = true;
        } else {
            mavenParameters->writeCSVFlag = false;
        }
        //Getting the classification model
        mavenParameters->clsf = mainwindow->getClassifier();

        mavenParameters->setCompounds(DB.getCompoundsSubset(
            compoundDatabase->currentText().toStdString()));

        mavenParameters->avgScanTime = settings->value("avgScanTime").toDouble();

        mavenParameters->samples = mainwindow->getSamples();

        peakupdater->setMavenParameters(mavenParameters);

    }
}
/**
 * PeakDetectionDialog::runBackgroupJob This function will start the thread for
 * the given algorithm with which the peak detection has to be done.
 * @param funcName [description]
 */
void PeakDetectionDialog::runBackgroupJob(QString funcName) {
        //Making sure all the setting that is necessary for starting the
        //thread is being added
        //TODO: have to add to check if the mavenParameters is not null
        if (peakupdater == NULL)
                return;
        // Making sure that the already some peakdetector thread is not
        // working
        if (peakupdater->isRunning()) {
                 cancel();
        }
        //Starting the thread
        if (!peakupdater->isRunning()) {
                peakupdater->setRunFunction(funcName); //set thread function
                peakupdater->start(); //start a background thread
        }
}

/**
 * PeakDetectionDialog::showInfo This updates the status process with the
 * algorithm with which thepeak detection is going to happen
 * @param text this is the algorithm that the peakdetection process uses
 */
void PeakDetectionDialog::showInfo(QString text) {
        statusText->setText(text);
}

/**
 * PeakDetectionDialog::setProgressBar This functionupdates the progress bar
 * with the percentage ofcomplition of peak detectionprocess
 * @param text       Text to be updated in the label
 * @param progress   Progress
 * @param totalSteps Total number of steps
 */
void PeakDetectionDialog::setProgressBar(QString text, int progress,
                                         int totalSteps) {
        showInfo(text);
        progressBar->setRange(0, totalSteps);
        progressBar->setValue(progress);
}

/*
@author:Sahil-Kiran
*/
void PeakDetectionDialog::showMethodSummary() {
    
    //Merged to 776
    if(peakupdater) {
        updateQSettingsWithUserInput(settings);
        setMavenParameters(settings);
        methodSummary->clear();
        methodSummary->setPlainText(peakupdater->printSettings());
    }
    //TODO: why does settings need to be updated right after reportIsotopes is checked/unchecked?
    //updateSettings after dialog close does not affect isotope detection but this does
    emit updateSettings(peakSettings);
}


/*
@author:Sahil-Kiran
*/
void PeakDetectionDialog::updatePeakTableList() {
    
    //merged to 776
    if(mainwindow) {
        QList< QPointer<TableDockWidget> > peaksTableList = mainwindow->getPeakTableList();
        for(int i=0; i < peaksTableList.size();i++) {
            QString tableName = peaksTableList[i]->objectName();
            if (outputTableComboBox->findText(tableName) == -1 ) {
                outputTableComboBox->addItem(tableName);
            }
        }
    }
}
