from scipy import stats
import numpy as np
import matplotlib.pyplot as plt

filename = '/root/fre64.txt'
ssd = []
ssdy = []
ssdsum=0
with open(filename, 'r') as file_to_read:
    lines = file_to_read.readline()
    while lines:
        a=lines.split()[1]
        b=lines.split()[3]
        a = int(a)
        b = int(b)
        ssdsum = ssdsum+b*a
        for x in range(b):
            ssd.append(a)
            ssdy.append(ssdsum)
        lines = file_to_read.readline()
np.sort(ssd)
ssdsum =float(ssdsum)
length=len(ssdy)
for i in range(length):
    ssdy[i] =  float(float(ssdy[i])/ssdsum)
fig, ax = plt.subplots()
plt.plot(ssd,ssdy,'-')
plt.margins(0.02)
ax.set_xlabel('frequency')
ax.set_ylabel('IOpercent')
#plt.xlim((0,200))
#plt.subplot(111)
#hist, bin_edges = np.histogram(ssd)
#cdf = np.cumsum(hist)
#plt.plot(cdf)
plt.show()
