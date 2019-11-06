#!/usr/bin/python
# -*- coding: UTF-8 -*-

blkdict={}
fre={}
fre[1]=0


class blk(object):
    def __init__(self, blkid, frequency):
        self.blkid = blkid
        self.frequency = frequency

if __name__ == "__main__":
    filename = '/mnt/blktrace/output'
    with open(filename, 'r') as file_to_read:
        lines = file_to_read.readline()
        while lines:
            lines = lines.strip()
            if not len(lines):
                continue
            if  (lines.split()[6]=='N'):
                lines = file_to_read.readline()
                continue
            a=lines.split()[7]
            b=lines.split()[9]
            a=int(a)
            a=a/8
            a=int(a)
            b=int(b)
            b=b/8
            b=int(b)
            for x in range(b):
                if a not in blkdict:
                    blkdict[a] = blk(a,1)
                    fre[1] = fre[1]+1
                    #fre[1].append(blkdict[a])
                else:
                    blk_obj = blkdict[a]
                    fre[blk_obj.frequency] -=1
                    if fre[blk_obj.frequency] == 0:
                        if not blk_obj.frequency == 1:
                            fre.pop(blk_obj.frequency)
                    blk_obj.frequency +=1
                    if blk_obj.frequency not in fre:
                        fre[blk_obj.frequency] = 1
                    else:
                        fre[blk_obj.frequency] += 1
                    #fre[blk_obj.frequency].remove(blk_obj)
                    #if fre[blk_obj.frequency]=[]:
                    #   fre.pop(blk_obj.frequency)
                    #blk_obj.frequency +=1
                    #blkdict[a]=blk_obj
                    #if blk_obj.frequency not in fre
                    #   fre.setdefault(blk_obj.frequency,[])
                    #fre[blk_obj.frequency].append(blk_obj)
                a=a+1
            lines = file_to_read.readline()
    output_file = open('/root/fre.txt','w')
    for v,k in fre.items():
   #     print('{v}:{k}'.format(v = v, k = k))
   # for key   in fre:
        output_file.write('frequency:  {v}    showtime:  {k}'.format(v = v, k = k))
        output_file.write('\n')
    output_file.close()
        
        
        
        
        
        
        
