# Report for Lab 5, Hao He

## Summary of Results

## Exercise 1

> **Exercise 1.** i386_init identifies the file system environment by passing the type ENV_TYPE_FS to your environment creation function, env_create. Modify env_create in env.c, so that it gives the file system environment I/O privilege, but never gives that privilege to any other environment. 

This can be implemented by adding the following code in `env_create()`.

```c
env->env_tf.tf_eflags &= ~FL_IOPL_MASK;
if (type == ENV_TYPE_FS) {
	env->env_tf.tf_eflags |= FL_IOPL_3;
} else {
	env->env_tf.tf_eflags |= FL_IOPL_0;
}
```

## Question 1

> Do you have to do anything else to ensure that this I/O privilege setting is saved and restored properly when you subsequently switch from one environment to another? Why? 

No, because each process has its own `EFLAGS` register, and their `IOPL` bits are no longer modified in other part of kernel code during context switch.

## Exercise 2