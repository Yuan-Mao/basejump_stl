#include <stdio.h>
#include <stdlib.h>
#include <math.h>

unsigned long sqrti(unsigned long a) {
  double a_double = double(a);
  a_double = sqrt(a_double);
  double a_double_int = rint(a_double);
//  printf("<%f %f>",a_double, a_double_int);
  return a_double_int;

}

double max(double a, double b)
{
  if (a > b)
    return a;
  else
    return b;
}

double minn(double a, double b)
{
  if (a < b)
    return a;
  else
    return b;
}


double cordic_scale_factor(int n)
{
  double scale = 1.0;

  for (int i = 0; i <= n; i++)
  {
    scale = scale / (sqrt(1.0+pow(2.0,-2.0*i)));
  }

  printf("scale factor %f\n", scale);
  return scale;
}

double scaling_factor = -1;
double scaling_factor_n = -1;

// this is the cordic algorithm
// there are two issues:
//   1. ASR(-1,1) = 1 and (ASR(-2,2) = -1).... (should be zero)
//   2. there are a lot of rounding errors (off by 1 digit); 
//      can be addressed by fixing precision.
//   3. we need to use a non-truncate rounding mode at the end
// the defines below can address both issues.
// the ASR might be too expensive in the circuit.
// you can change these and evaluate the error rates.

// ultimately both double sqrt and this algorithm
// will have error due to rounding. for the double sqrt,
// the error is about .5
//
// We measure error, and also absolute error above
// and beyond the double sqrt quantization error.
//

// #define extra_precision 10
// gives a max error of 1.24 %
//

// #define extra_precision 6
// keeps absolute error below .58
// (best possible is .5)

#define extra_precision 6

#define USE_ROUNDING_MODE 1

#define FIX_ASR 0

#define kFixVal 1
#define kFixOfs 1

// use this to print out data on errors.
#define REPORT_ERRORS 0

#define ASR(a,i) 
int cordic(int x, int y, int n, int verbose)
{
  if (scaling_factor_n != n)
  {
    scaling_factor = cordic_scale_factor(n+1);
    scaling_factor_n = n;
  }

  if (x < y)
  {
    x = x ^ y;
    y = y ^ x;  // (=x ^ y ^ y = x)
    x = x ^ y;  // (=x^y^x = y)
  }

  x = x << extra_precision;
  y = y << extra_precision;
  
  if (verbose)
    printf ("(%d %d) ", x, y);

  for (int i = 0; i <= n; i++)
  {
    int x_n = x, y_n = y;

    if (y >= 0)
    {
      // y is not negative here, and x is not negative (by definition)
      // so no worries about incorrect semantics of ASR.
      x_n = x + (y >> i);
      y_n = y - (x >> i);
    }
    else
    {
      int y_tmp = y >> i;
      // this is awkward, but
      // to implement a correctly rounded
      // arithmetic shift, we need to check for this case.

#if FIX_ASR
      if (y_tmp == -1)
      {
        if (i && ((y >> (i-1)) == -1))
        {
          y_tmp = 0;
        }
      }
#endif
      x_n = x - y_tmp;
      y_n = y + (x >> i);
    }
    x = x_n;
    y = y_n;
    if (verbose)
      printf("(%d %d) ",x,y);
  }

  if (verbose)
    printf ("@%f-->%f@\n"
            ,(scaling_factor * x)/ (1 << extra_precision)
            ,((scaling_factor * x) + (1 << (extra_precision-1))+kFixVal) / (1 << extra_precision));

  // to eliminate the trunc rounding mode with integer calculations,
  // we add a binary .5 before we shift off the low bits.
  if (USE_ROUNDING_MODE)
    return (( (int) ( scaling_factor * x)) + (1 << (extra_precision-kFixOfs)) + kFixVal)   >> extra_precision;
  else
    return (( (int) ( scaling_factor * x))) >> extra_precision;
}

// iterates through whole space and computes error


int try_cordic()
{
  int start = 1;
  int bits = 12;
  double max_error_percent = 0.0;
  double max_additional_error = 0.0;
  double max_error_val = 0;
  int total_errors = 0;
  int total_trials = 0;
  for (int x = start; x < (1 << bits); x++)
    for (int y = start; y < (1 << bits); y++)
    {
      total_trials++;
      int distance = cordic(x,y,bits,0);
      int exact = sqrti(x*x + y*y);

      double error = (double (exact - distance)) / double (exact);
      error = fabs(error*100);
      if (distance != exact)
        total_errors++;

      //if (max_error == error)
//      if (error > 0.0)
      {
        double actual_sqrt = sqrt(x*x + y*y);
        double additional_error = fabs(((double) distance)-actual_sqrt)-fabs(((double)exact)-actual_sqrt);


        if ((error > 0.0) && (REPORT_ERRORS))
        {

          printf(" QQ %d %d -> %d %d (%f) error = %f, addtl error %f\n"
                 ,x,y
                 ,distance, exact,actual_sqrt,error, additional_error);

          cordic(x,y,bits,1);
          printf("\n\n");
        }

        double error_val = fabs(actual_sqrt - distance);
        max_error_val = max(max_error_val,error_val);
        max_error_percent = max(max_error_percent,error_val/actual_sqrt*100.0);
        max_additional_error = max(max_additional_error,additional_error);

      }
      // printf("(%d,%d)-> %d (%d) %f\n",x,y,distance,exact, error);
    }
  printf("total errors = %d (%d) -> 1 in %d; max abs error val = %f, max addtl abs error=%f, max error pct = %f\n",total_errors, 
         total_trials, (total_trials/total_errors),
         max_error_val, max_additional_error,max_error_percent);
}


int tryit(int seg_begin, int seg_end, int bits, int min, int y_mul_base, int y_mul_delta, int x_mul_base, int x_mul_delta)
{
  double min_max_error = 30000000;

  for (int y_mul = y_mul_base - y_mul_delta; y_mul <= (y_mul_base+y_mul_delta); y_mul++)
    for (int x_mul = x_mul_base - x_mul_delta; x_mul <= (x_mul_base+x_mul_delta); x_mul++)
    {
      double max_error = 0.0;
      for (int x = min; x < (1 << bits); x++)
        for (int y = x >> seg_begin; y < (x >> seg_end); y++)
       {
          int result = (y_mul * y + x_mul * x + 512) >> 10;
          int exact = sqrti (x*x + y*y);

          
          double error = (double (exact - result)) / double (exact);
          error = fabs(error*100);
          if (error > 3.40)
            printf("%d %d %d %d %g\n",x,y,result,exact,error);
          max_error = max(error,max_error);
        }

      min_max_error = minn(max_error,min_max_error);
      if (min_max_error == max_error)
        printf("max error for  (%d,%d,%d,%d) %f\n",seg_begin,seg_end,y_mul,x_mul,max_error);
    }
}

int main(int argc, char **argv[])
{
  // 967, 1007, 441

  //  seg_begin, seg_end, bits, min, y_mul_base, y_mul_delta, x_mul_base, x_mul_delta
//  tryit(2,1,12,80,456,0,942,0);

  // printf("%d\n",cordic(1160,1042,12));
  try_cordic();

#if 0 
  for (int const_a = 956*kMult; const_a <= 956*kMult; const_a++)
    for (int const_b = 1005*kMult; const_b <= 1005*kMult; const_b++)
      for (int const_c = 439*kMult; const_c <= 439*kMult; const_c++)
        for (int const_d = 954*kMult; const_d <= 954*kMult; const_d++)
  {
    int bits = 12;
    int min = 40;
    double max_error = 0.0;

    for (int x = (1 << bits) - 1; x >= min; x--)
      for (int y = min; y <= x; y++)
      {
        int result = const_c*y;
        int x_mul;

        if (x < ((y << 1) + (y >> 2)))
          x_mul = const_d;
        else
          if (x < ((y << 4) - 3*y))
            x_mul = const_a;
          else
            x_mul = const_b;

        result += x_mul * x;
        result = ((result + 512*kMult) >> 10) / kMult;

        if (x == y)
          result = (int) (( (float) x) * 1.4142);


        int exact = sqrti (x*x + y*y);

        double error = (double (exact - result)) / double (exact);
        error = fabs(error*100);

        max_error = max(error,max_error);

        if (error > 3.8)
          printf ("%f %d %d %f\n", error, x,y,(double) x / (double) y );
        //Printf("%d %d * %d %d %f\n",x,y,result,exact, error);

      }
    min_max_error = minn(max_error,min_max_error);
    if (min_max_error == max_error)
      printf("max error for  (%d,%d,%d,%d) %f\n",const_a,const_b,const_c,const_d, max_error);
  }
  printf ("min error: %f\n",min_max_error);
#endif
}
