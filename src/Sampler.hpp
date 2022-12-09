#pragma once

namespace CycleSampler
{

    template<int AmbDim, typename Real, typename Int>
    class RandomVariable;
    
    template<typename Real = double, typename Int = long long>
    struct SamplerSettings
    {
        Real tolerance            = std::sqrt(std::numeric_limits<Real>::epsilon());
        Real give_up_tolerance    = 100 * std::numeric_limits<Real>::epsilon();
        Real regularization       = 1.;
        Int  max_iter             = 1000;
        
        Real Armijo_slope_factor  = 0.01;
        Real Armijo_shrink_factor = 0.5;
        Int  max_backtrackings    = 20;
        
        bool use_linesearch       = true;
        
        SamplerSettings() {}
        
        ~SamplerSettings() = default;
        
        SamplerSettings( const SamplerSettings & other )
        :   tolerance(other.tolerance)
        ,   give_up_tolerance(other.give_up_tolerance)
        ,   regularization(other.regularization)
        ,   max_iter(other.max_iter)
        ,   Armijo_slope_factor(other.Armijo_slope_factor)
        ,   Armijo_shrink_factor(other.Armijo_shrink_factor)
        ,   max_backtrackings(other.max_backtrackings)
        ,   use_linesearch(other.use_linesearch)
        {}
        
        void PrintStats() const
        {
            valprint( "tolerance           ", tolerance           , 16 );
            valprint( "give_up_tolerance   ", give_up_tolerance   , 16 );
            valprint( "regularization      ", regularization      , 16 );
            valprint( "max_iter            ", max_iter            , 16 );
            valprint( "Armijo_slope_factor ", Armijo_slope_factor , 16 );
            valprint( "Armijo_shrink_factor", Armijo_shrink_factor, 16 );
            valprint( "max_backtrackings   ", max_backtrackings   , 16 );
            valprint( "use_linesearch      ", use_linesearch      , 16 );
        }
    };
    
    template<int AmbDim, typename Real = double, typename Int = long long>
    class Sampler
    {
        ASSERT_FLOAT(Real);
        ASSERT_INT(Int);
        
    public:
        
        using Vector_T             = Tiny::Vector         <AmbDim,Real,Int>;
        using SquareMatrix_T       = Tiny::SquareMatrix   <AmbDim,Real,Int>;
        using SymmetricMatrix_T    = Tiny::SelfAdjointMatrix<AmbDim,Real,Int>;
        
        using RandomVariable_T     = RandomVariable<AmbDim,Real,Int>;
        
        using SpherePoints_T = Tensor2<Real,Int>;
        using SpacePoints_T  = Tensor2<Real,Int>;
        using Weights_T      = Tensor1<Real,Int>;
        using Setting_T      = SamplerSettings<Real,Int>;
        
        Sampler()
        {
            random_engine = std::mt19937_64( std::random_device()() );
        }
        
        
        ~Sampler(){}
        
        explicit Sampler(
            const Int edge_count_,
            const Setting_T settings_ = Setting_T()
        )
        :   edge_count(edge_count_)
        ,   settings(settings_)
        ,   x   ( edge_count,     AmbDim )
        ,   y   ( edge_count,     AmbDim )
        ,   p   ( edge_count + 1, AmbDim )
        ,   r   ( edge_count, one / edge_count )
        ,   rho ( edge_count, one )
        ,   total_r_inv ( one )
        {
            std::random_device r;
            
            std::seed_seq seed { r(), r(), r(), r()   };
            
            random_engine = std::mt19937_64( seed );
        }
        
        explicit Sampler(
            const Real * restrict const r_in,
            const Real * restrict const rho_in,
            const Int edge_count_,
            const Setting_T settings_ = Setting_T()
        )
        :   edge_count(edge_count_)
        ,   settings(settings_)
        ,   x   ( edge_count,     AmbDim )
        ,   y   ( edge_count,     AmbDim )
        ,   p   ( edge_count + 1, AmbDim )
        ,   r   ( edge_count )
        ,   rho ( rho_in, edge_count )
        {
            ReadEdgeLengths(r_in);
            
            std::random_device r;
            
            std::seed_seq seed { r(), r(), r(), r()   };
            
            random_engine = std::mt19937_64( seed );
        }
        
        
        
        // Copy constructor
        Sampler( const Sampler & other )
        :   edge_count( other.edge_count )
        ,   settings( other.settings )
        ,   x(other.x)
        ,   y(other.y)
        ,   p(other.p)
        ,   r(other.r)
        ,   rho(other.rho)
        ,   total_r_inv(other.total_r_inv)
        ,   w(other.w)
        ,   F(other.F)
        ,   DF(other.DF)
        ,   L(other.L)
        ,   u(other.u)
        ,   z(other.z)
        ,   iter(other.iter)
        ,   squared_residual(other.squared_residual)
        ,   residual(other.residual)
        ,   edge_space_sampling_weight(other.edge_space_sampling_weight)
        ,   edge_quotient_space_sampling_correction(other.edge_quotient_space_sampling_correction)
        ,   lambda_min(other.lambda_min)
        ,   q(other.q)
        ,   errorestimator(errorestimator)
        ,   linesearchQ(linesearchQ)
        ,   succeededQ(succeededQ)
        ,   continueQ(continueQ)
        ,   ArmijoQ(ArmijoQ)
        {}
        
        friend void swap(Sampler &A, Sampler &B) noexcept
        {
            // see https://stackoverflow.com/questions/5695548/public-friend-swap-member-function for details
            using std::swap;
            swap(A.edge_count,B.edge_count);
            swap(A.settings,B.settings);
            swap(A.x,B.x);
            swap(A.y,B.y);
            swap(A.p,B.p);
            swap(A.r,B.r);
            swap(A.rho,B.rho);
            swap(A.total_r_inv,B.total_r_inv);
            swap(A.w,B.w);
            swap(A.F,B.F);
            swap(A.A,B.A);
            swap(A.u,B.u);
            swap(A.z,B.z);

            swap(A.iter,             B.iter             );
            swap(A.squared_residual, B.squared_residual );
            swap(A.residual,         B.residual         );
            swap(A.edge_space_sampling_weight,A.edge_space_sampling_weight);
            swap(A.edge_quotient_space_sampling_correction,A.edge_quotient_space_sampling_correction);
            swap(A.lambda_min,B.lambda_min);
            swap(A.q,B.q);
            swap(A.errorestimator,B.errorestimator);
            swap(A.linesearchQ,B.linesearchQ);
            swap(A.succeededQ,B.succeededQ);
            swap(A.continueQ,B.continueQ);
            swap(A.ArmijoQ,B.ArmijoQ);
        }
        
        // Copy assignment operator
        Sampler & operator=(Sampler other)
        {
            // copy-and-swap idiom
            // see https://stackoverflow.com/a/3279550/8248900 for details
            swap(*this, other);

            return *this;
        }

        /* Move constructor */
        Sampler( Sampler && other ) noexcept
        :   Sampler()
        {
            swap(*this, other);
        }
        
        
    protected:
        
        const Int edge_count = 0;
        
        mutable std::mt19937_64 random_engine;
        
        mutable std::normal_distribution<Real> normal_dist {static_cast<Real>(0),static_cast<Real>(1)};
        
        Setting_T settings;
        
        SpherePoints_T x {0,AmbDim};
        SpherePoints_T y {0,AmbDim};
        SpacePoints_T  p {0,AmbDim};
        
        Weights_T      r {0};
        Weights_T    rho {0};
        
        Real total_r_inv = one;
        
        Vector_T w;           // current point in hyperbolic space.
        Vector_T F;           // right hand side of Newton iteration.
        SymmetricMatrix_T DF; // nabla F(0) with respect to measure ys
        SymmetricMatrix_T L;  // storing Cholesky factor.
        Vector_T u;           // update direction
        
        Vector_T z;           // Multiple purpose buffer.
        
        Int iter = 0;
        
        Real squared_residual = 1;
        Real         residual = 1;
        
        Real edge_space_sampling_weight = 0;
        Real edge_quotient_space_sampling_correction = 0;
        
        Real lambda_min = eps;
        Real q = one;
        Real errorestimator = infty;
        
        bool linesearchQ = true;    // Toggle line search.
        bool succeededQ  = false;   // Whether algorithm has succeded.
        bool continueQ   = true;    // Whether to continue with the main loop.
        bool ArmijoQ     = false;   // Whether Armijo condition was met last time we checked.
        
        static constexpr Real zero              = 0;
        static constexpr Real half              = 0.5;
        static constexpr Real one               = 1;
        static constexpr Real two               = 2;
        static constexpr Real three             = 3;
        static constexpr Real four              = 4;
        static constexpr Real eps               = std::numeric_limits<Real>::min();
        static constexpr Real infty             = std::numeric_limits<Real>::max();
        static constexpr Real small_one         = 1 - 16 * eps;
        static constexpr Real big_one           = 1 + 16 * eps;
        static constexpr Real g_factor          = 4;
        static constexpr Real g_factor_inv      = one/g_factor;
        static constexpr Real norm_threshold    = 0.99 * 0.99 + 16 * eps;
        static constexpr Real two_pi            = static_cast<Real>(2 * M_PI);
        
    public:
        
        void Optimize()
        {
            const Int max_iter = settings.max_iter;
            
            iter = 0;
            
            Shift();
            
            DifferentialAndHessian_Hyperbolic();
            
            SearchDirection_Hyperbolic();
            
            // The main loop.
            while( ( iter < max_iter ) && continueQ )
            {
                ++iter;
                
                LineSearch_Hyperbolic_Potential();
                
                DifferentialAndHessian_Hyperbolic();
                
                SearchDirection_Hyperbolic();
            }
        }
        
        Int EdgeCount() const
        {
            return edge_count;
        }
        
        Real EdgeSpaceSamplingWeight()
        {
            return edge_space_sampling_weight;
        }
        
        Real EdgeQuotientSpaceSamplingCorrection()
        {
            return edge_quotient_space_sampling_correction;
        }
        
        Real EdgeQuotientSpaceSamplingWeight()
        {
            return EdgeSpaceSamplingWeight() * EdgeQuotientSpaceSamplingCorrection();
        }
        
        
    protected:
        
        Real Potential( const Vector_T & z )
        {
            const Real zz = Dot(z,z);
            
            const Real a = big_one + zz;
            const Real c = (big_one-zz);
            
            const Real b = one/c;
            
            Real value = 0;
            
            for( Int k = 0; k < edge_count; ++k )
            {
                Vector_T y_k( y[k] );
                
                value += r[k] * std::log( std::abs( (a - two * Dot(y_k,z) ) * b ) );
            }
            
            return value * total_r_inv;
        }
        
        
        void LineSearch_Hyperbolic_Residual()
        {
            // 2 F(0)^T.DF(0).u is the derivative of w\mapsto F(w)^T.F(w) at w = 0.
            const Real slope = two * DF.InnerProduct(F,u);
            
            Real tau = one;
            
            const Real u_norm = u.Norm();
            
            // exponential map shooting from 0 to tau * u.
            Times( tau * tanhc(tau * u_norm), u, z );
            
            // TODO: Isn't it vice versa?
            // Shift the point z along -w to get new updated point w
            InverseShift(z,w);
            
            // Shift the input measure along w to 0 to simplify gradient, Hessian, and update computation .
            Shift();
            
            const Real squared_residual_at_0 = squared_residual;
            
            DifferentialAndHessian_Hyperbolic();
            
            if( linesearchQ )
            {
                Int backtrackings = 0;
                
                // Armijo condition for squared residual.
                
                ArmijoQ = squared_residual - squared_residual_at_0 - settings.Armijo_slope_factor * tau * slope < zero;
                
                while( !ArmijoQ && (backtrackings < settings.max_backtrackings) )
                {
                    ++backtrackings;
                    
                    // Estimate step size from quadratic fit if applicable.
                    
                    const Real tau_1 = settings.Armijo_shrink_factor * tau;
                    const Real tau_2 = - half * settings.Armijo_slope_factor * tau * tau * slope / ( squared_residual  - squared_residual_at_0 - tau * slope );
                    
                    tau = std::max( tau_1, tau_2 );
                    
                    Times( tau * tanhc(tau * u_norm), u, z );
                    
                    // TODO: Isn't it vice versa?
                    // Shift the point z along -w to get new updated point w .
                    InverseShift(z,w);
                    
                    // Shift the input measure along w to 0 to simplify gradient, Hessian, and update computation .
                    Shift();
                    
                    DifferentialAndHessian_Hyperbolic();
                    
                    ArmijoQ = squared_residual - squared_residual_at_0 - settings.Armijo_slope_factor * tau * slope < 0;
                }
            }
        }
        
        void LineSearch_Hyperbolic_Potential()
        {
            Real tau = one;
            
            const Real u_norm = u.Norm();
            
            // exponential map shooting from 0 to tau * u.
            
            Times( tau * tanhc(tau * u_norm), u, z );
            
            if( linesearchQ )
            {
                //Linesearch with potential as merit function.
                
                const Real gamma = settings.Armijo_shrink_factor;
                
                const Real sigma = settings.Armijo_slope_factor;
                
                const Real Dphi_0 = g_factor * Dot(F,u);
                
                Int backtrackings = 0;
                
                // Compute potential and check Armijo condition.
                
                //                const Real phi_0 = Potential(o);
                
                Real phi_tau = Potential(z);
                
                ArmijoQ = phi_tau /*- phi_0*/ - sigma * tau * Dphi_0 < 0;
                
                
                while( !ArmijoQ && (backtrackings < settings.max_backtrackings) )
                {
                    ++backtrackings;
                    
                    const Real tau_1 = gamma * tau;
                    
                    // Estimate step size from quadratic fit if applicable.
                    const Real tau_2 = - half * sigma * tau * tau * Dphi_0 / ( phi_tau /*- phi_0*/ - tau * Dphi_0 );
                    
                    tau = std::max( tau_1, tau_2 );
                    
                    Times( tau * tanhc(tau * u_norm), u, z );
                    
                    phi_tau = Potential(z);
                    
                    ArmijoQ = phi_tau  /*- phi_0*/ - sigma * tau * Dphi_0 < 0;
                }
            }
            
            // TODO: Isn't it vice versa?
            // Shift the point z along -w to get new updated point w .
            InverseShift(z,w);
            
            // Shift the input measure along w to 0 to simplify gradient, Hessian, and update computation .
            Shift();
        }
        
        void DifferentialAndHessian_Hyperbolic()
        {
            // CAUTION: We use a different sign convention as in the paper!
            // Assemble  F = -1/2 y * r.
            // Assemble DF = nabla F + regulatization:
            // DF_{ij} = \delta_{ij} - \sum_k x_{k,i} x_{k,j} \r_k.
            
            // Filling F and DF with first summand...
            {
                for( Int i = 0; i < AmbDim; ++i )
                {
                    const Real factor = r[0] * y[0][i];
                    
                    F[i] = - factor;
                    
                    for( Int j = i; j < AmbDim; ++j )
                    {
                        DF[i][j] = - factor * y[0][j];
                    }
                }
            }
            
            // ... and adding-in the other summands.
            for( Int k = 1; k < edge_count; ++k )
            {
                for( Int i = 0; i < AmbDim; ++i )
                {
                    const Real factor = r[k] * y[k][i];
                    
                    F[i] -= factor;
                    
                    for( Int j = i; j < AmbDim; ++j )
                    {
                        DF[i][j] -= factor * y[k][j];
                    }
                }
            }
            
            // Normalize for case that the weights in r do not sum to 1.
            
            F  *= total_r_inv;
            DF *= total_r_inv;
            
            squared_residual = Dot(F,F);
            
            residual = std::sqrt( squared_residual );
            
            F *= half;
            
            // Better add the identity afterwards for precision reasons.
            for( Int i = 0; i < AmbDim; ++i )
            {
                DF[i][i] += one;
            }
        }
        
        void SearchDirection_Hyperbolic()
        {
            // Make decisions whether to continue.
            if( residual < static_cast<Real>(100.) * settings.tolerance )
            {
                // We have to compute eigenvalue _before_ we add the regularization.
                lambda_min = DF.SmallestEigenvalue();
                q = four * residual / (lambda_min * lambda_min);
                
                if( q < one )
                {
                    //Kantorovich condition satisfied; this allows to compute an error estimator.
                    errorestimator = half * lambda_min * q;
                    //And we should deactivate line search. Otherwise, we may run into precision issues.
                    linesearchQ = false;
                    continueQ = (errorestimator >settings.tolerance);
                    succeededQ = !continueQ;
                }
                else
                {
                    errorestimator = infty;
                    linesearchQ = settings.Armijo_slope_factor > zero;
                    //There is no way to reduce the residual below machine epsilon. If the algorithm reaches here, the problem is probably too ill-conditioned to be solved in machine precision.
                    continueQ = residual > settings.give_up_tolerance;
                }
            }
            else
            {
                q = big_one;
                lambda_min = eps;
                errorestimator = infty;
                linesearchQ = settings.Armijo_slope_factor > zero;
                continueQ = residual>std::max( settings.give_up_tolerance, settings.tolerance );
            }
            
            const Real c = settings.regularization * squared_residual;
            
            for( Int i = 0; i < AmbDim; ++i )
            {
                for( Int j = i; j < AmbDim; ++j )
                {
                    L[i][j] = DF[i][j] + static_cast<Real>(i==j) * c;
                }
            }
            
            L.Cholesky();
            
            L.CholeskySolve(F,u);
            
            u *= -one;
        }
        
        void Gradient_Hyperbolic()
        {
            Times(-g_factor_inv, F, u);
        }
        
        void Gradient_Planar()
        {
            // The factor 2 is here to reproduce the Abikoff-Ye algorithm (in the absence of linesearch.)
            Times(-two, F, u);
        }
        
        void InverseShift( const Vector_T & z, Vector_T & w )
        {
            // Shift point w along -z.
            
            const Real ww  = Dot(w,w);
            const Real wz2 = Dot(w,z) * two;
            const Real zz  = Dot(z,z);
            
            const Real a = one - ww;
            const Real b = one + zz + wz2;
            const Real c = big_one + wz2 + ww * zz;
            const Real d = one / c;
            
            for( Int i = 0; i < AmbDim; ++i )
            {
                w[i] = ( a * z[i] + b * w[i] ) * d;
            }
        }
        
        void Shift(
        //            const SpherePoints_T & x_in,
        //            const Vector_T       & w,
        //                  SpherePoints_T & y_out,
        //            const Real             t = one
        )
        {
            // Shifts all entries of x along w and writes the results to y.
            
            const Real ww = Dot(w,w);
            const Real one_minus_ww = big_one - ww;
            const Real one_plus_ww  = big_one + ww;
            
            if( ww <= norm_threshold )
            {
                for( Int k = 0; k < edge_count; ++k )
                {
                    Vector_T x_k ( x[k] );
                    
                    const Real wx2 = two * Dot(w,x_k);
                    
                    const Real denom = one / ( one_plus_ww - wx2 );
                    
                    const Real wx2_minus_2 = wx2 - two;
                    
                    for( Int i = 0; i < AmbDim; ++i )
                    {
                        y(k,i) = (one_minus_ww * x_k[i] + wx2_minus_2 * w[i]) * denom;
                    }
                }
            }
            else
            {
                // If w lies close to the boundary of the ball, then normalizing the output is a good idea.
                
                for( Int k = 0; k < edge_count; ++k )
                {
                    Vector_T x_k ( x[k] );
                    
                    const Real wx2 = two * Dot(w,x_k);
                    
                    const Real denom = one / ( one_plus_ww - wx2 );
                    
                    const Real wx2_minus_2 = wx2 - two;
                    
                    for( Int i = 0; i < AmbDim; ++i )
                    {
                        x_k[i] = (one_minus_ww * x_k[i] + wx2_minus_2 * w[i]) * denom;
                    }
                    
                    x_k.Normalize();
                    
                    x_k.Write(y[k]);
                }
            }
        }
        
        void ComputeEdgeSpaceSamplingWeight()
        {
            // Shifts all entries of x along y and writes the results to y.
            // Mind that x and y are stored in SoA fashion, i.e., as matrix of size AmbDim x point_count.
            
            SquareMatrix_T cbar  (0);
            SquareMatrix_T gamma (0);
            
            Real prod = one;
            
            const Real ww = Dot(w,w);
            
            const Real one_plus_ww = big_one + ww;
            
            for( Int k = 0; k < edge_count; ++k )
            {
                Vector_T y_k ( y[k] );
                
                const Real wy = Dot(w,y_k);
                
                const Real factor = one_plus_ww + two * wy;
                
                // Multiplying by one_plus_ww_inv so that prod does not grow so quickly.
                prod *= factor;
                
                const Real r_k = r[k];
                const Real r_over_rho_k = r_k / rho[k];
                const Real r_over_rho_k_squared = r_over_rho_k * r_over_rho_k;
                
                for( Int i = 0; i < AmbDim; ++i )
                {
                    for( Int j = 0; j < AmbDim; ++j )
                    {
                        const Real scratch = (static_cast<Real>(i==j) - y_k[i] *  y_k[j] );
                        
                        gamma[i][j] += r_over_rho_k_squared * scratch;
                        
                        cbar [i][j] += r_k * scratch;
                    }
                }
            }
            
            //            We can simply absorb the factor std::pow(2/(one_minus_ww),d) into the function chi.
            //            cbar *= static_cast<Real>(2)/(one_minus_ww);
            
            edge_space_sampling_weight = MyMath::pow(prod, static_cast<Int>(AmbDim-1)) * sqrt(gamma.Det()) / cbar.Det();
        }
        
        void ComputeEdgeQuotientSpaceSamplingCorrection()
        {
            if constexpr ( AmbDim == 2)
            {
                edge_quotient_space_sampling_correction = one;
                return;
            }
            
            Eigen::Matrix<Real,AmbDim,AmbDim> Sigma;
            
            // We fill only the lower triangle of Sigma, because that's the only thing that Eigen's selfadjoint eigensolver needs.
            // Recall that Eigen matrices are column-major by default.
            
            // Overwrite for k = 0.
            {
                const Real rho_squared = rho[0] * rho[0];
                for( Int i = 0; i < AmbDim; ++i )
                {
                    const Real factor = rho_squared * y(0,i);
                    
                    for( Int j = i; j < AmbDim; ++j )
                    {
                        Sigma(j,i) = factor * y(0,j);
                    }
                }
            }
            
            // Beware, we have to start here at k = 1 because we already overwrote the matrix.
            for( Int k = 1; k < edge_count; ++k )
            {
                const Real rho_squared = rho[k] * rho[k];
                for( Int i = 0; i < AmbDim; ++i )
                {
                    const Real factor = rho_squared * y(k,i);
                    
                    for( Int j = i; j < AmbDim; ++j )
                    {
                        Sigma(j,i) += factor * y(k,j);
                    }
                }
            }
            
            // Eigen needs only the lower triangular part. So need not symmetrize.
            
            //            for( Int i = 0; i < AmbDim; ++i )
            //            {
            //                for( Int j = 0; j < i; ++j )
            //                {
            //                    Sigma(j,i) = Sigma(i,j);
            //                }
            //            }
            
            if constexpr ( AmbDim == 3)
            {
                // Exploiting that
                //      (lambda[0] + lambda[1]) * (lambda[0] + lambda[2]) * (lambda[1] + lambda[2])
                //      =
                //      ( tr(Sigma*Sigma) - tr(Sigma)*tr(Sigma) ) *  tr(Sigma)/2 - det(Sigma)
                //  Thus, it can be expressed by as third-order polynomial in the entries of the matrix.
                
                const Real S_00 = Sigma(0,0)*Sigma(0,0);
                const Real S_11 = Sigma(1,1)*Sigma(1,1);
                const Real S_22 = Sigma(2,2)*Sigma(2,2);
                
                const Real S_10 = Sigma(1,0)*Sigma(1,0);
                const Real S_20 = Sigma(2,0)*Sigma(2,0);
                const Real S_21 = Sigma(2,1)*Sigma(2,1);
                
                const Real det = std::abs(
                      Sigma(0,0) * ( S_11 + S_22 - S_10 - S_20 )
                    + Sigma(1,1) * ( S_00 + S_22 - S_10 - S_21 )
                    + Sigma(2,2) * ( S_00 + S_11 - S_20 - S_21 )
                    + two * (Sigma(0,0)*Sigma(1,1)*Sigma(2,2) - Sigma(1,0)*Sigma(2,0)*Sigma(2,1))
                );
                edge_quotient_space_sampling_correction = one / std::sqrt(det);
                return;
            }
            
            Eigen::SelfAdjointEigenSolver< Eigen::Matrix<Real,AmbDim,AmbDim> > eigs;
            
            eigs.compute(Sigma);
            
            auto & lambda = eigs.eigenvalues();
            
            Real det = one;
            
            for( Int i = 0; i < AmbDim; ++i )
            {
                for( Int j = i+1; j < AmbDim; ++j )
                {
                    det *= (lambda(i)+lambda(j));
                }
            }
            
            edge_quotient_space_sampling_correction = one / std::sqrt(det);
        }
        
    public:
        
        
        const SpherePoints_T & InitialEdgeCoordinates() const
        {
            return x;
        }
        
        void ReadInitialEdgeCoordinates( const Real * const x_in, bool normalize = true )
        {
            if( normalize )
            {
                for( Int k = 0; k < edge_count; ++k )
                {
                    Vector_T x_k ( x_in[AmbDim * k ] );
                    
                    x_k.Normalize();
                    
                    x_k.Write( x[k] );
                }
            }
            else
            {
                x.Read( x_in );
            }
        }
        
        void ReadInitialEdgeCoordinates( const Real * const x_in, const Int k, bool normalize = true )
        {
            ReadInitialEdgeCoordinates( &x_in[ AmbDim * edge_count * k], normalize);
        }
        
        void WriteInitialEdgeCoordinates( Real * x_out ) const
        {
            x.Write(x_out);
        }
        
        void WriteInitialEdgeCoordinates( Real * x_out, const Int k ) const
        {
            WriteInitialEdgeCoordinates( &x_out[ AmbDim * edge_count * k ]);
        }
        
        void RandomizeInitialEdgeCoordinates()
        {
            for( Int k = 0; k < edge_count; ++k )
            {
                Vector_T x_k;
                
                for( Int i = 0; i < AmbDim; ++i )
                {
                    x_k[i] = normal_dist( random_engine );
                }
                
                x_k.Normalize();
                
                x_k.Write( x[k] );
            }
        }
        
        
        
        const SpherePoints_T & EdgeCoordinates() const
        {
            return y;
        }
        
        void ReadEdgeCoordinates( const Real * const y_in )
        {
            y.Read(y_in);
        }
        
        void ReadEdgeCoordinates( const Real * const y_in, const Int k )
        {
            ReadEdgeCoordinates( &y_in[ AmbDim * edge_count * k ]);
        }
        
        void WriteEdgeCoordinates( Real * y_out ) const
        {
            y.Write(y_out);
        }
        
        void WriteEdgeCoordinates( Real * y_out, const Int k ) const
        {
            WriteEdgeCoordinates( &y_out[ AmbDim * edge_count * k ]);
        }
        
        
        
        const SpacePoints_T & SpaceCoordinates() const
        {
            return p;
        }
        
        void WriteSpaceCoordinates( Real * p_out ) const
        {
            p.Write(p_out);
        }
        
        void WriteSpaceCoordinates( Real * p_out, const Int k ) const
        {
            WriteSpaceCoordinates( &p_out[ (edge_count+1) * AmbDim * k ]);
        }
        
        void ComputeSpaceCoordinates()
        {
            //Caution: This gives only have the weight to the end vertices of the chain.
            //Thus this is only really the barycenter, if the chain is closed!
            
            Real barycenter        [AmbDim] = {};
            Real point_accumulator [AmbDim] = {};
            
            for( Int k = 0; k < edge_count; ++k )
            {
                const Real r_k = r[k];
                
                for( Int i = 0; i < AmbDim; ++i )
                {
                    const Real offset = r_k * y(k,i);
                    
                    barycenter[i] += (point_accumulator[i] + half * offset);
                    
                    point_accumulator[i] += offset;
                }
            }
            
            for( Int i = 0; i < AmbDim; ++i )
            {
                p(0,i) = -barycenter[i]/edge_count;
            }
            
            for( Int k = 0; k < edge_count; ++k )
            {
                const Real r_k = r[k];
                
                for( Int i = 0; i < AmbDim; ++i )
                {
                    p(k+1,i) = p(k,i) + r_k * y(k,i);
                }
            }
        }
        
        
        const Weights_T & EdgeLengths() const
        {
            return r;
        }
        
        void ReadEdgeLengths( const Real * const r_in )
        {
            r.Read(r_in);
            
            total_r_inv = one / r.Total();
        }
        
        
        const Weights_T & Rho() const
        {
            return rho;
        }
        
        void ReadRho( const Real * const rho_in )
        {
            rho.Read(rho_in);
        }
        
        
        void ComputeShiftVector()
        {

            // Overwrite by first summand.
            {
                const Real r_k = r[0];
                
                for( Int i = 0; i < AmbDim; ++i )
                {
                    w[i] = x(0,i) * r_k;
                }
            }
            
            // Add-in the others.
            for( Int k = 1; k < edge_count; ++k )
            {
                const Real r_k = r[k];
                
                for( Int i = 0; i < AmbDim; ++i )
                {
                    w[i] += x(k,i) * r_k;
                }
            }
            
            // Normalize in that case that r does not sum up to 1.
            w *= total_r_inv;
        }
        
        void ReadShiftVector( const Real * const w_in )
        {
            w.Read(w_in);
            
            // Use Euclidean barycenter as initial guess if the supplied initial guess does not make sense.
            if( Dot(w,w) > small_one )
            {
                ComputeShiftVector();
            }
        }
        
        void ReadShiftVector( const Real * const w_in, const Int k )
        {
            ReadShiftVector( &w_in[ AmbDim * k ] );
        }
        
        void WriteShiftVector( Real * w_out ) const
        {
            w.Write(w_out);
        }
        
        void WriteShiftVector( Real * w_out, const Int k ) const
        {
            w.Write(&w_out[ AmbDim * k]);
        }
        
        const Vector_T & ShiftVector() const
        {
            return w;
        }
        
        
        Real Residual() const
        {
            return residual;
        }
        
        Real ErrorEstimator() const
        {
            return errorestimator;
        }
        
        Int IterationCount() const
        {
            return iter;
        }
        
        Int MaxIterationCount() const
        {
            return settings.max_iter;
        }
        
        
    public:
        
        void OptimizeBatch(
            const Real * restrict const x_in,
                  Real * restrict const w_out,
                  Real * restrict const y_out,
            const                       Int sample_count,
            const                       Int thread_count = 1,
            const bool                  normalize = true
        )
        {
            ptic(ClassName()+"OptimizeBatch");
            
            JobPointers<Int> job_ptr ( sample_count, thread_count );
            
            #pragma omp parallel for num_threads( thread_count )
            for( Int thread = 0; thread < thread_count; ++thread )
            {
                const Int k_begin = job_ptr[thread];
                const Int k_end   = job_ptr[thread+1];
                
                Sampler W( edge_count, settings );
                
                W.ReadEdgeLengths( EdgeLengths().data() );
                
                for( Int k = k_begin; k < k_end; ++k )
                {
                    W.ReadInitialEdgeCoordinates( x_in, k, normalize );
                    
                    W.ComputeShiftVector();
                    
                    W.Optimize();
                    
                    W.WriteShiftVector( w_out, k );
                    
                    W.WriteEdgeCoordinates( y_out, k );
                }
            }
            
            ptoc(ClassName()+"OptimizeBatch");
        }
        
        void RandomClosedPolygons(
                  Real * restrict const x_out,
                  Real * restrict const w_out,
                  Real * restrict const y_out,
                  Real * restrict const K_edge_space,
                  Real * restrict const K_edge_quotient_space,
            const Int                   sample_count,
            const Int                   thread_count = 1
        ) const
        {
            ptic(ClassName()+"RandomClosedPolygons");
            
            JobPointers<Int> job_ptr ( sample_count, thread_count );
            
            #pragma omp parallel for num_threads( thread_count )
            for( Int thread = 0; thread < thread_count; ++thread )
            {
                const Int k_begin = job_ptr[thread  ];
                const Int k_end   = job_ptr[thread+1];
                
                Sampler W( EdgeLengths().data(), Rho().data(), edge_count, settings );
                
                for( Int k = k_begin; k < k_end; ++k )
                {
                    W.RandomizeInitialEdgeCoordinates();
                    
                    W.WriteInitialEdgeCoordinates(x_out, k);
                    
                    W.ComputeShiftVector();
                    
                    W.Optimize();
                    
                    W.WriteShiftVector(w_out,k);
                    
                    W.WriteEdgeCoordinates(y_out,k);
                    
                    W.ComputeEdgeSpaceSamplingWeight();
                    
                    W.ComputeEdgeQuotientSpaceSamplingCorrection();
                    
                    K_edge_space[k] = W.EdgeSpaceSamplingWeight();
                    
                    K_edge_quotient_space[k] = W.EdgeQuotientSpaceSamplingWeight();
                }
            }
            
            ptoc(ClassName()+"::RandomClosedPolygons");
        }
        
        
        // moments: A 3D-array of size 3 x fun_count x bin_count. Entry moments(i,j,k) will store the sampled weighted k-th moment of the j-th random variable from the list F_list -- with respect to the weights corresponding to the value of i (see above).
        // ranges: Specify the range for binning: For j-th function in F_list, the range from ranges(j,0) to ranges(j,1) will be devided into bin_count bins. The user is supposed to provide meaningful ranges. Some rough guess might be obtained by calling the random variables on the prepared Sampler_T C.
        
        void Sample_Binned(
                 Real * restrict bins_out,
           const Int             bin_count_,
                 Real * restrict moments_out,
           const Int             moment_count_,
           const Real * restrict ranges,
           const std::vector< std::unique_ptr<RandomVariable_T> > & F_list_,
           const Int             sample_count,
           const Int             thread_count = 1
        ) const
        {
            ptic(ClassName()+"Sample_Binned");
            
            const Int fun_count = static_cast<Int>(F_list_.size());
            
            const Int moment_count = std::max( static_cast<Int>(3), moment_count_ );
            
            const Int bin_count = std::max( bin_count_, static_cast<Int>(1) );
            
            JobPointers<Int> job_ptr ( sample_count, thread_count );
            
            valprint( "dimension   ", AmbDim       );
            valprint( "edge_count  ", edge_count );
            valprint( "sample_count", sample_count );
            valprint( "fun_count   ", fun_count );
            valprint( "bin_count   ", bin_count );
            valprint( "moment_count", moment_count );
            valprint( "thread_count", thread_count );
            
            
            Tensor3<Real,Int> bins_global   ( bins_out,    3, fun_count, bin_count    );
            Tensor3<Real,Int> moments_global( moments_out, 3, fun_count, moment_count );
            Tensor1<Real,Int> factor        (                 fun_count               );
            
            print("Sampling the following random variables:");
            for( Int i = 0; i < fun_count; ++ i )
            {
                const size_t i_ = static_cast<size_t>(i);
                factor(i) = static_cast<Real>(bin_count) / ( ranges[2*i+1] - ranges[2*i+0] );
                
                print("    " + F_list_[i_]->Tag());
            }
            
            const Int lower = static_cast<Int>(0);
            const Int upper = static_cast<Int>(bin_count-1);
            
            #pragma omp parallel for num_threads( thread_count )
            for( Int thread = 0; thread < thread_count; ++thread )
            {
                const Int repetitions = (job_ptr[thread+1] - job_ptr[thread]);
                
                Sampler W( EdgeLengths().data(), Rho().data(), edge_count, settings );
                
                std::vector< std::unique_ptr<RandomVariable_T> > F_list;
                
                for( Int i = 0; i < fun_count; ++ i )
                {
                    F_list.push_back(
                        std::unique_ptr<RandomVariable_T>(
                           dynamic_cast<RandomVariable_T*>( F_list_[i]->Clone().release() )
                        )
                    );
                }
                
                Tensor3<Real,Int> bins_local   ( 3, fun_count, bin_count,    zero );
                Tensor3<Real,Int> moments_local( 3, fun_count, moment_count, zero );
                
                for( Int k = 0; k < repetitions; ++k )
                {
                    W.RandomizeInitialEdgeCoordinates();
                    
                    W.ComputeShiftVector();
                    
                    W.Optimize();
                    
                    W.ComputeSpaceCoordinates();
                    
                    const Real K = W.EdgeSpaceSamplingWeight();
                    
                    const Real K_quot = W.EdgeQuotientSpaceSamplingWeight();
                    
                    for( Int i = 0; i < fun_count; ++i )
                    {
                        const Real val = (*F_list[i])(W);
                        
                        Real values [3] = { one, K, K_quot };
                        
                        const Int bin_idx = static_cast<Int>(
                            std::floor( factor[i] * (val - ranges[2*i]) )
                        );
                        
                        if( (bin_idx <= upper) && (bin_idx >= lower) )
                        {
                            bins_local(0,i,bin_idx) += one;
                            bins_local(1,i,bin_idx) += K;
                            bins_local(2,i,bin_idx) += K_quot;
                        }
                        
                        moments_local(0,i,0) += values[0];
                        moments_local(1,i,0) += values[1];
                        moments_local(2,i,0) += values[2];
                        
                        for( Int j = 1; j < moment_count; ++j )
                        {
                            values[0] *= val;
                            values[1] *= val;
                            values[2] *= val;
                            moments_local(0,i,j) += values[0];
                            moments_local(1,i,j) += values[1];
                            moments_local(2,i,j) += values[2];
                        }
                    }
                }
                
                #pragma omp critical
                {
                    add_to_buffer(
                        bins_local.data(), bins_global.data(), 3 * fun_count * bin_count
                    );
                    
                    add_to_buffer(
                        moments_local.data(), moments_global.data(), 3 * fun_count * moment_count
                    );
                }
            }
            
            bins_global.Write( bins_out );
            moments_global.Write( moments_out );
            
            ptoc(ClassName()+"::Sample_Binned");
        }
        
        void NormalizeBinnedSamples(
                  Real * restrict bins,
            const Int             bin_count,
                  Real * restrict moments,
            const Int             moment_count,
            const Int             fun_count
        ) const
        {
            ptic(ClassName()+"::NormalizeBinnedSamples");
            for( Int i = 0; i < 3; ++i )
            {
                for( Int j = 0; j < fun_count; ++j )
                {
                    // Normalize bins and moments.
                    
                    Real * restrict const bins_i_j = &bins[ (i*fun_count+j)*bin_count ];
                    
                    Real * restrict const moments_i_j = &moments[ (i*fun_count+j)*moment_count ];
                    
                    // The field for zeroth moment is assumed to contain the total mass.
                    Real factor = Real(1)/moments_i_j[0];
                    
                    scale_buffer( factor, bins_i_j,    bin_count    );
                    
                    scale_buffer( factor, moments_i_j, moment_count );
                }
            }
            ptoc(ClassName()+"::NormalizeBinnedSamples");
        }
        
#if defined(PLCTOPOLOGY_H)
        
        std::map<std::string, std::tuple<Real,Real,Real>> SampleHOMFLY(
            const Int sample_count,
            const Int thread_count = 1
        ) const
        {
            ptic(ClassName()+"SampleHOMFLY");
            
            std::map<std::string, std::tuple<Real,Real,Real>> map_global;
            
            if( AmbDim != 3 )
            {
                eprint("SampleHOMFLY is only available in 3D.");
                return map_global;
            }
            
            JobPointers<Int> job_ptr (sample_count, thread_count);
            
            valprint( "dimension   ", AmbDim       );
            valprint( "edge_count  ", edge_count   );
            valprint( "sample_count", sample_count );
            valprint( "thread_count", thread_count );
            
            gsl_rng_env_setup();
            
            Tensor1<unsigned long,Int> seeds ( thread_count );
            
            std::random_device r;
            
            for( Int thread = 0 ; thread < thread_count; ++ thread )
            {
                seeds[thread] = (std::numeric_limits<unsigned int>::max()+1) * r() + r();
            }
            
            #pragma omp parallel for num_threads( thread_count )
            for( Int thread = 0; thread < thread_count; ++thread )
            {
                const Int repetitions = (job_ptr[thread+1] - job_ptr[thread]);
                
                Sampler W( edge_count, EdgeLengths().data(), Rho().data(), settings );
                
                std::map<std::string, std::tuple<Real,Real,Real>> map_loc;
                
                bool openQ = false;
                
                int color_count = 0;
                
                int n = static_cast<int>(edge_count);
                
                gsl_rng *rng = gsl_rng_alloc( gsl_rng_mt19937 );
                
                gsl_rng_set( rng, seeds[thread] );
                
                plCurve * Gamma = plc_new( 1, &n, &openQ, &color_count );
                //
                //                auto * p = &Gamma->cp[0].vt[0];
                
                auto & p = W.SpaceCoordinates();
                
                for( Int l = 0; l < repetitions; ++l )
                {
                    //                    valprint("l",l);
                    
                    W.RandomizeInitialEdgeCoordinates();
                    
                    W.ComputeShiftVector();
                    
                    W.Optimize();
                    
                    W.ComputeSpaceCoordinates();
                    
                    W.ComputeEdgeSpaceSamplingWeight();
                    
                    W.ComputeEdgeQuotientSpaceSamplingCorrection();
                    
                    const Real K = W.EdgeSpaceSamplingWeight();
                    
                    const Real K_quot = W.EdgeQuotientSpaceSamplingWeight();
                    
                    auto * comp = &Gamma->cp[0];
                    
                    for( int k = 0; k < edge_count; ++k )
                    {
                        auto * v = &comp->vt[k].c[0];
                        
                        for( int i = 0; i < 3; ++i )
                        {
                            v[i] = p(k,i);
                        }
                    }
                    
                    plc_fix_wrap( Gamma );
                    
                    
                    //                    char * polynomial = (char *) malloc( sizeof(char) * 3);
                    //
                    //                    polynomial[0] = 'a';
                    //                    polynomial[1] = 'b';
                    //                    polynomial[2] = 'c';
                    
                    char * polynomial = plc_homfly( rng, Gamma );
                    
                    std::string s("");
                    
                    if( polynomial != nullptr )
                    {
                        s.append(polynomial);
                        free( polynomial );
                    }
                    else
                    {
                        s.append("FAILED");
                    }
                    
                    //                    s << polynomial;
                    //
                    //                    std::string str = s.str();
                    //                    print(s);
                    
                    map_loc.try_emplace(s, std::tie(zero,zero,zero) );
                    
                    auto & tuple = map_loc[s];
                    
                    std::get<0>(tuple) += one;
                    std::get<1>(tuple) += K;
                    std::get<2>(tuple) += K_quot;
                }
                
                #pragma omp critical
                {
                    for ( auto const & [key, val] : map_loc )
                    {
                        map_global.try_emplace( key, std::tie(zero,zero,zero) );
                        
                        auto & from = val;
                        auto & to   = map_global[key];
                        
                        std::get<0>(to) += std::get<0>(from);
                        std::get<1>(to) += std::get<1>(from);
                        std::get<2>(to) += std::get<2>(from);
                    }
                }
                
                gsl_rng_free( rng );
                
                plc_free( Gamma );
            }
            
            
            ptoc(ClassName()+"::SampleHOMFLY");
            
            return map_global;
        }
        
#endif
        
    protected:
        
        Real tanhc( const Real t ) const
        {
            // Computes tanh(t)/t in a stable way by using a Padé approximation around t = 0.
            constexpr Real a0 = static_cast<Real>(1);
            constexpr Real a1 = static_cast<Real>(7)/static_cast<Real>(51);
            constexpr Real a2 = static_cast<Real>(1)/static_cast<Real>(255);
            constexpr Real a3 = static_cast<Real>(2)/static_cast<Real>(69615);
            constexpr Real a4 = static_cast<Real>(1)/static_cast<Real>(34459425);
            
            constexpr Real b0 = static_cast<Real>(1);
            constexpr Real b1 = static_cast<Real>(8)/static_cast<Real>(17);
            constexpr Real b2 = static_cast<Real>(7)/static_cast<Real>(255);
            constexpr Real b3 = static_cast<Real>(4)/static_cast<Real>(9945);
            constexpr Real b4 = static_cast<Real>(1)/static_cast<Real>(765765);
            
            const Real t2 = t * t;
            
            const Real result = ( t2 <= one )
            ? (
                a0 + t2 * (a1 + t2 * (a2 + t2 * (a3 + t2 * a4)))
            )/(
                b0 + t2 * (b1 + t2 * (b2 + t2 * (b3 + t2 * b4)))
            )
            : ( t2 <= static_cast<Real>(7) ) ? std::tanh(t)/t : one/std::abs(t);
            
            return result;
        }
        
    public:
        
        Int AmbientDimension() const
        {
            return AmbDim;
        }
        
    public:
        
        const Setting_T & Settings() const
        {
            return settings;
        }
        
        
        std::string ClassName() const
        {
            return "Sampler<"+ToString(AmbDim)+","+TypeName<Real>::Get()+","+TypeName<Int>::Get()+","+">";
        }
        
    }; // class Sampler
    
} // namespace CycleSampler
