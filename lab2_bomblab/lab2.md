# CSAPP lab2 bomblab 实验报告

## 1 实验解题思路

### 1.5 phase_5
```asm
mov    (%rsp),%eax  # eax=(rsp)=x
and    $0xf,%eax # 提取x后四位 %16
mov    %eax,(%rsp)
cmp    $0xf,%eax  
je     1858 <phase_5+0x71>
```