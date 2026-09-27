#pragma once
#include <cstdarg>
namespace Eloquent {
    namespace ML {
        namespace Port {
            class DecisionTree {
                public:
                    /**
                    * Predict class for features vector
                    */
                    int predict(float *x) {
                        if (x[1] <= 59.5) {
                            if (x[2] <= 0.5) {
                                return 4;
                            }

                            else {
                                return 0;
                            }
                        }

                        else {
                            if (x[1] <= 72.5) {
                                if (x[2] <= 0.5) {
                                    return 2;
                                }

                                else {
                                    return 3;
                                }
                            }

                            else {
                                return 1;
                            }
                        }
                    }

                    /**
                    * Predict readable class name
                    */
                    const char* predictLabel(float *x) {
                        return idxToLabel(predict(x));
                    }

                    /**
                    * Convert class idx to readable name
                    */
                    const char* idxToLabel(uint8_t classIdx) {
                        switch (classIdx) {
                            case 0:
                            return "discomfort_hot";
                            case 1:
                            return "discomfort_humid";
                            case 2:
                            return "intrusion";
                            case 3:
                            return "normal";
                            case 4:
                            return "occupied_uncomfortable";
                            default:
                            return "Houston we have a problem";
                        }
                    }

                protected:
                };
            }
        }
    }