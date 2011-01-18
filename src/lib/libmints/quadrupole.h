#ifndef _psi_src_lib_libmints_quadrupole_h_
#define _psi_src_lib_libmints_quadrupole_h_

#include <vector>
#include <boost/shared_ptr.hpp>

namespace psi {

class OneBodyInt;
class ObaraSaikaTwoCenterRecursion;
class GaussianShell;
class SphericalTransform;
class BasisSet;
class Matrix;
class SimpleMatrix;

/*! \ingroup MINTS
 *  \class QuadrupoleInt
 *  \brief Computes quadrupole integrals. At last check this may not be working.
 *  Use an IntegralFactory to create this object.
 */
class QuadrupoleInt : public OneBodyInt
{
    ObaraSaikaTwoCenterRecursion overlap_recur_;

    void compute_pair(const boost::shared_ptr<GaussianShell>&, const boost::shared_ptr<GaussianShell>&);

public:
    QuadrupoleInt(std::vector<SphericalTransform>&, boost::shared_ptr<BasisSet>, boost::shared_ptr<BasisSet>);
    virtual ~QuadrupoleInt();

    /// Computes all quadrupole integrals (Qxx, Qxy, Qxz, Qyy, Qyz, Qzz) result must be an array of enough
    /// size to contain it.
    void compute(std::vector<boost::shared_ptr<Matrix> > &result);
    void compute(std::vector<boost::shared_ptr<SimpleMatrix> > &result);
};

}

#endif
