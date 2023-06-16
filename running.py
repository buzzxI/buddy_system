import subprocess

# Define the commands to execute
commands = [
    'make clean',
    'make all',
    './buddy_system'
]

# Execute each command in sequence
for command in commands:
    try:
        subprocess.run(command, shell=True, check=True)
    except subprocess.CalledProcessError as e:
        print(f"python: Error executing command: {command}")
        print(f"python: Return code: {e.returncode}")
        print(f"python: Output: {e.output.decode()}")
        break

