#ifndef B972_C_MATLAB_FUNCTIONAL_H
#define B972_C_MATLAB_FUNCTIONAL_H

#include "functional.h"

namespace psi {

/** 
 * B972_C MATLAB Autogenerated functional 
 **/

class B972_CFunctional : public Functional {

public:

    B972_CFunctional();
    virtual ~B972_CFunctional(); 
    virtual void compute_functional(const std::map<std::string,SharedVector>& in, const std::map<std::string,SharedVector>& out, int npoints, int deriv, double alpha);

};

}

#endif
