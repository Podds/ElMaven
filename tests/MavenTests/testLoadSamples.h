#ifndef TESTLOADSAMPLES_H
#define TESTLOADSAMPLES_H
#include <iostream>
#include <QtTest>
#include <string>
#include <sstream>
#include "utilities.h"

class TestLoadSamples : public QObject {
    Q_OBJECT

    public:
        TestLoadSamples();
    private:
        const char* loadFile;
        const char* blankSample;

    private Q_SLOTS:
        // functions executed by QtTest before and after test suite
        void initTestCase();
        void cleanupTestCase();

        // functions executed by QtTest before and after each test
        void init();
        void cleanup();

        // test functions - all functions prefixed with "test" will be ran as tests
        // this is automatically detected thanks to Qt's meta-information about QObjects
        void testFileLoad();
        void testIsAllScansParsed();
        void testScanParsing();
        void testSrmScan();
        void testMinMaxMz();
        void testMinMaxRT();
        void testSampleName();
        void testBlankSample();
        void testParseMzMLInjectionTimeStamp();
};

#endif // TESTLOADSAMPLES_H