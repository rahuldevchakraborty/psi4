#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <utility>

#include <psifiles.h>
#include <libciomr/libciomr.h>
#include <libpsio/psio.hpp>
#include <libchkpt/chkpt.hpp>
#include <libiwl/iwl.hpp>
#include <libqt/qt.h>
#include "integralfunctors.h"

#include <libmints/mints.h>
#include "rohf.h"
#include <psi4-dec.h>

#define _DEBUG

using namespace std;
using namespace psi;
using namespace boost;

namespace psi { namespace scf {

ROHF::ROHF(Options& options, shared_ptr<PSIO> psio, shared_ptr<Chkpt> chkpt)
    : HF(options, psio, chkpt)
{
    common_init();
}

ROHF::ROHF(Options& options, shared_ptr<PSIO> psio)
    : HF(options, psio)
{
    common_init();
}

ROHF::~ROHF() {
}

void ROHF::common_init()
{
    Fa_      = SharedMatrix(factory_->create_matrix("Alpha Fock Matrix"));
    Fb_      = SharedMatrix(factory_->create_matrix("Beta Fock Matrix"));
    Feff_    = SharedMatrix(factory_->create_matrix("F effective (MO basis)"));
    Ca_      = SharedMatrix(factory_->create_matrix("Molecular orbitals"));
    Cb_      = Ca_;
    Da_      = SharedMatrix(factory_->create_matrix("Alpha density matrix"));
    Db_      = SharedMatrix(factory_->create_matrix("Beta density matrix"));
    Ka_      = SharedMatrix(factory_->create_matrix("K alpha"));
    Kb_      = SharedMatrix(factory_->create_matrix("K beta"));
    Ga_      = SharedMatrix(factory_->create_matrix("G alpha"));
    Gb_      = SharedMatrix(factory_->create_matrix("G beta"));
    Dt_old_  = SharedMatrix(factory_->create_matrix("D alpha old"));
    Dt_      = SharedMatrix(factory_->create_matrix("D beta old"));
    moFa_    = SharedMatrix(factory_->create_matrix("MO Basis alpha Fock Matrix"));
    moFb_    = SharedMatrix(factory_->create_matrix("MO Basis beta Fock Matrix"));

    epsilon_a_ = SharedVector(factory_->create_vector());
    epsilon_b_ = epsilon_a_;

    fprintf(outfile, "  DIIS %s.\n\n", diis_enabled_ ? "enabled" : "disabled");
}

void ROHF::finalize()
{
    Feff_.reset();
    Ka_.reset();
    Kb_.reset();
    Ga_.reset();
    Gb_.reset();
    Dt_old_.reset();
    Dt_.reset();
    moFa_.reset();
    moFb_.reset();

    HF::finalize();
}

void ROHF::save_density_and_energy()
{
    Dt_old_->copy(Dt_);
    Eold_ = E_; // save previous energy
}

void ROHF::save_information()
{
}

void ROHF::save_fock()
{
    if (initialized_diis_manager_ == false) {
        diis_manager_ = shared_ptr<DIISManager>(new DIISManager(max_diis_vectors_, "HF DIIS vector", DIISManager::LargestError, DIISManager::OnDisk, psio_));
        diis_manager_->set_error_vector_size(1, DIISEntry::Matrix, Feff_.get());
        diis_manager_->set_vector_size(1, DIISEntry::Matrix, Feff_.get());
        initialized_diis_manager_ = true;
    }

    // Dear DIIS person (with my luck, future Rob):
    // Remember that X is nso x nmo, so an MO or orthogonal basis F matrix is not the same size as the SO basis
}

bool ROHF::diis()
{
    return diis_manager_->extrapolate(1, Feff_.get());
}

bool ROHF::test_convergency()
{
    // energy difference
    double ediff = E_ - Eold_;

    // RMS of the density
    Matrix D_rms;
    D_rms.copy(Dt_);
    D_rms.subtract(Dt_old_);
    Drms_ = D_rms.rms();

    if (fabs(ediff) < energy_threshold_ && Drms_ < density_threshold_)
        return true;
    else
        return false;
}

void ROHF::form_initialF()
{
    // Form the initial Fock matrix, closed and open variants
    Fa_->copy(H_);
    Fa_->transform(X_);
    Fb_->copy(Fa_);

#ifdef _DEBUG
    if (debug_) {
        fprintf(outfile, "Initial alpha Fock matrix:\n");
        Fa_->print(outfile);
        fprintf(outfile, "Initial beta Fock matrix:\n");
        Fb_->print(outfile);
    }
#endif
}

void ROHF::form_F()
{
    // Start by constructing the standard Fa and Fb matrices encountered in UHF
    Fa_->copy(H_);
    Fb_->copy(H_);
    Fa_->add(Ga_);
    Fb_->add(Gb_);

    moFa_->transform(Fa_, Ca_);
    moFb_->transform(Fb_, Ca_);

    /*
     * Fo = open-shell fock matrix = 0.5 Fa
     * Fc = closed-shell fock matrix = 0.5 (Fa + Fb)
     *
     * Therefore
     *
     * 2(Fc-Fo) = Fb
     * 2Fo = Fa
     *
     * Form the effective Fock matrix, too
     * The effective Fock matrix has the following structure
     *          |  closed     open    virtual
     *  ----------------------------------------
     *  closed  |    Fc     2(Fc-Fo)    Fc
     *  open    | 2(Fc-Fo)     Fc      2Fo
     *  virtual |    Fc       2Fo       Fc
     */
    Feff_->copy(moFa_);
    Feff_->add(moFb_);
    Feff_->scale(0.5);
    for (int h = 0; h < nirrep_; ++h) {
        for (int i = doccpi_[h]; i < doccpi_[h] + soccpi_[h]; ++i) {
            // Set the open/closed portion
            for (int j = 0; j < doccpi_[h]; ++j) {
                double val = moFb_->get(h, i, j);
                Feff_->set(h, i, j, val);
                Feff_->set(h, j, i, val);
            }
            // Set the open/virtual portion
            for (int j = doccpi_[h] + soccpi_[h]; j < nmopi_[h]; ++j) {
                double val = moFa_->get(h, i, j);
                Feff_->set(h, i, j, val);
                Feff_->set(h, j, i, val);
            }
        }
    }

    if (debug_) {
        Fa_->print();
        Fb_->print();
        moFa_->print();
        moFb_->print();
        Feff_->print(outfile);
    }
}

void ROHF::form_C()
{
    SharedMatrix temp(factory_->create_matrix());
    SharedMatrix eigvec(factory_->create_matrix());

    // Obtain new eigenvectors
    Feff_->diagonalize(eigvec, epsilon_a_);
    find_occupation();

    if (debug_) {
        fprintf(outfile, "In ROHF::form_C:\n");
        eigvec->eivprint(epsilon_a_);
    }
    temp->gemm(false, false, 1.0, Ca_, eigvec, 0.0);
    Ca_->copy(temp);

    if (debug_) {
        Ca_->print(outfile);
    }
}

void ROHF::form_initial_C()
{
    SharedMatrix temp = shared_ptr<Matrix>(factory_->create_matrix());

    // In ROHF the creation of the C matrix depends on the previous iteration's C
    // matrix. Here we use Fa to generate the first C, where Fa was set by guess()
    // to either H or the GWH Hamiltonian.
    temp->copy(Fa_);
    temp->transform(X_);
    temp->diagonalize(Ca_, epsilon_a_);
    find_occupation();
    temp->gemm(false, false, 1.0, X_, Ca_, 0.0);
    Ca_->copy(temp);

    if (print_ > 3)
        Ca_->print(outfile, "initial C");
}

void ROHF::form_D()
{
    for (int h = 0; h < nirrep_; ++h) {
        int nso = nsopi_[h];
        int nmo = nmopi_[h];
        int na = nalphapi_[h];
        int nb = nbetapi_[h];
    
        if (nso == 0 || nmo == 0 || na == 0) continue;

        double** Ca = Ca_->pointer(h);
        double** Da = Da_->pointer(h);
        double** Db = Db_->pointer(h);

        C_DGEMM('N','T',nso,nso,na,1.0,Ca[0],nmo,Ca[0],nmo,0.0,Da[0],nso);
        C_DGEMM('N','T',nso,nso,nb,1.0,Ca[0],nmo,Ca[0],nmo,0.0,Db[0],nso);

    }

    Dt_->copy(Da_);
    Dt_->add(Db_);

    if (debug_) {
        fprintf(outfile, "in ROHF::form_D:\n");
        Da_->print();
        Db_->print();
    }
}

double ROHF::compute_initial_E()
{
    return 0.5 * (compute_E() + nuclearrep_);
}

double ROHF::compute_E() {
    double DH  = Da_->vector_dot(H_);
    DH += Db_->vector_dot(H_);
    double DFa = Da_->vector_dot(Fa_);
    double DFb = Db_->vector_dot(Fb_);
    double Eelec = 0.5 * (DH + DFa + DFb);
    double Etotal = nuclearrep_ + Eelec;
    return Etotal;
}

void ROHF::form_G()
{
    /*
     * This just builds the same Ga and Gb matrices used in UHF
     */
    J_Ka_Kb_Functor jk_builder(Ga_, Ka_, Kb_, Da_, Db_, Ca_, Cb_, nalphapi_, nbetapi_);
    process_tei<J_Ka_Kb_Functor>(jk_builder);

    Gb_->copy(Ga_);
    Ga_->subtract(Ka_);
    Gb_->subtract(Kb_);
}

}}
