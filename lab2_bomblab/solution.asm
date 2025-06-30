00000000000019e7 <secret_phase>:
    19e7:	f3 0f 1e fa          	endbr64 
    19eb:	53                   	push   %rbx
    19ec:	e8 87 02 00 00       	call   1c78 <read_line>
    19f1:	48 89 c7             	mov    %rax,%rdi  # rdi=输入
    19f4:	ba 0a 00 00 00       	mov    $0xa,%edx
    19f9:	be 00 00 00 00       	mov    $0x0,%esi
    19fe:	e8 dd f8 ff ff       	call   12e0 <strtol@plt>
    1a03:	89 c3                	mov    %eax,%ebx    # ebx = eax = return value
    1a05:	83 e8 01             	sub    $0x1,%eax
    1a08:	3d e8 03 00 00       	cmp    $0x3e8,%eax
    1a0d:	77 26                	ja     1a35 <secret_phase+0x4e>
    1a0f:	89 de                	mov    %ebx,%esi   # rsi = rbx = 输入数
    1a11:	48 8d 3d 18 37 00 00 	lea    0x3718(%rip),%rdi        # 5130 <n1>
    1a18:	e8 89 ff ff ff       	call   19a6 <fun7>
    1a1d:	83 f8 04             	cmp    $0x4,%eax
    1a20:	75 1a                	jne    1a3c <secret_phase+0x55>
    1a22:	48 8d 3d 57 17 00 00 	lea    0x1757(%rip),%rdi        # 3180 <_IO_stdin_used+0x180>
    1a29:	e8 f2 f7 ff ff       	call   1220 <puts@plt>
    1a2e:	e8 7d 03 00 00       	call   1db0 <phase_defused>
    1a33:	5b                   	pop    %rbx
    1a34:	c3                   	ret    
    1a35:	e8 cd 01 00 00       	call   1c07 <explode_bomb>
    1a3a:	eb d3                	jmp    1a0f <secret_phase+0x28>
    1a3c:	e8 c6 01 00 00       	call   1c07 <explode_bomb>
    1a41:	eb df                	jmp    1a22 <secret_phase+0x3b>

    00000000000019a6 <fun7>:
    19a6:	f3 0f 1e fa          	endbr64 
    19aa:	48 85 ff             	test   %rdi,%rdi           # (%rdi) = 36
    19ad:	74 32                	je     19e1 <fun7+0x3b>    # rdi 不能为0
    19af:	48 83 ec 08          	sub    $0x8,%rsp           # (%rsp) = 64
    19b3:	8b 17                	mov    (%rdi),%edx         # edx = 36
    19b5:	39 f2                	cmp    %esi,%edx           # esi = 输入数
    19b7:	7f 0c                	jg     19c5 <fun7+0x1f>
    19b9:	b8 00 00 00 00       	mov    $0x0,%eax
    19be:	75 12                	jne    19d2 <fun7+0x2c>  
    19c0:	48 83 c4 08          	add    $0x8,%rsp
    19c4:	c3                   	ret    
    19c5:	48 8b 7f 08          	mov    0x8(%rdi),%rdi
    19c9:	e8 d8 ff ff ff       	call   19a6 <fun7>
    19ce:	01 c0                	add    %eax,%eax            # 叠加
    19d0:	eb ee                	jmp    19c0 <fun7+0x1a>
    19d2:	48 8b 7f 10          	mov    0x10(%rdi),%rdi   # (%rdi) = 50
    19d6:	e8 cb ff ff ff       	call   19a6 <fun7>
    19db:	8d 44 00 01          	lea    0x1(%rax,%rax,1),%eax
    19df:	eb df                	jmp    19c0 <fun7+0x1a>
    19e1:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
    19e6:	c3                   	ret    
