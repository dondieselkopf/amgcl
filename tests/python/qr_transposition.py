#!/usr/bin/python

from scipy import *
from scipy.linalg import *
from numpy import *

A = random.randn(3, 3)
Q, R = linalg.qr(A)

print norm(dot(Q,R) - A)

AT = transpose(A)

print norm(dot(transpose(R),transpose(Q)) - AT)
