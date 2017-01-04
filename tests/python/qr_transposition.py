#!/usr/bin/python

from scipy import *
from scipy.linalg import *
from numpy import *

n = 5
m = 3

A = random.randn(n,m)
AT = transpose(A)
Q, R = linalg.qr(A)
print "A dim: ", A.shape
print "Q dim: ", Q.shape
print "R dim: ", R.shape
print norm(dot(Q,R) - A)

for i in xrange(0,n-1):
    for j in xrange(0,m-1):
        sum = 0
        for k in xrange(0,m-1):
            sum += R[k,j]*Q[i,k]
        sum -= AT[j,i]
        assert abs(sum)<1e-8,"Error in QR factorization"

#AT = transpose(A)
#print norm(dot(transpose(R),transpose(Q)) - AT)
