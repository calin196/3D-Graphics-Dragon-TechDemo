#version 410

uniform sampler2D positionAge;
uniform sampler2D velocityLifetime;
uniform float timestep;
uniform uint simulationStep;
uniform bool resetParticles;
uniform vec3 emitterPositions[3];
uniform vec3 emitterForward;
uniform bool breathActive;
uniform bool tailActive;
uniform vec3 tailPositions[64];
uniform vec3 tailDirections[64];
layout(location = 0) out vec4 nextPositionAge;
layout(location = 1) out vec4 nextVelocityLifetime;

float randomValue(uint seed)
{
    seed ^= seed >> 16u;
    seed *= 0x7feb352du;
    seed ^= seed >> 15u;
    seed *= 0x846ca68bu;
    seed ^= seed >> 16u;
    return float(seed & 0x00ffffffu) / 16777216.0;
}

void main()
{
    ivec2 stateCell = ivec2(gl_FragCoord.xy);
    int stateWidth = textureSize(positionAge, 0).x;
    int stateIndex = stateCell.y * stateWidth + stateCell.x;
    uint particleId = uint(stateIndex);

    // Sixty-four embers share the fire state textures and update pass.
    if (stateIndex >= 1024) {
        int emitterIndex = clamp(stateIndex - 1024, 0, 63);
        vec3 tailPosition = tailPositions[emitterIndex];
        vec3 tailBackward = tailDirections[emitterIndex];
        vec4 emberPosition = texelFetch(positionAge, stateCell, 0);
        vec4 emberVelocity = texelFetch(velocityLifetime, stateCell, 0);
        if (resetParticles || !tailActive || stateIndex >= 1088) {
            float spawnDelay = 0.45 * randomValue(particleId + 1u);
            nextPositionAge = vec4(tailPosition, -spawnDelay);
            nextVelocityLifetime = vec4(0);
            return;
        }
        float age = emberPosition.w + timestep;
        if (age >= emberVelocity.w && age >= 0.0) {
            uint seed = particleId * 1664525u + simulationStep * 1013904223u;
            float sideways = randomValue(seed) - 0.5;
            float depthDrift = randomValue(seed + 1u) - 0.5;
            vec3 drift = vec3(sideways * 0.30, 0.16, depthDrift * 0.20);
            vec3 velocity = tailBackward * 0.35 + drift;
            float lifetime = mix(0.45, 0.70, randomValue(seed + 2u));
            vec3 spawnScatter = vec3(sideways * 0.025, 0.0, depthDrift * 0.025);
            emberPosition.xyz = tailPosition + spawnScatter;
            emberVelocity = vec4(velocity, lifetime);
            age = 0.0;
        }
        if (age >= 0.0 && emberVelocity.w > 0.0) {
            emberPosition.xyz += emberVelocity.xyz * timestep;
        }
        nextPositionAge = vec4(emberPosition.xyz, age);
        nextVelocityLifetime = emberVelocity;
        return;
    }


    uint streamSlot = particleId % 10u;
    int stream;
    if (streamSlot < 6u) {
        stream = 0;
    } else if (streamSlot < 8u) {
        stream = 1;
    } else {
        stream = 2;
    }
    vec3 emitterPosition = emitterPositions[stream];
    vec4 positionAgeState = texelFetch(positionAge, stateCell, 0);
    vec4 velocityLifetimeState = texelFetch(velocityLifetime, stateCell, 0);
    if (resetParticles || !breathActive) {

        nextPositionAge = vec4(emitterPosition, -0.85 * randomValue(particleId + 1u));
        nextVelocityLifetime = vec4(0);
        return;
    }
    float age = positionAgeState.w + timestep;
    float previousLifetime = velocityLifetimeState.w;
    bool shouldSpawn;
    if (velocityLifetimeState.w <= 0.0 && age >= 0.0) {
        shouldSpawn = true;
    } else {
        shouldSpawn = velocityLifetimeState.w > 0.0 && age >= velocityLifetimeState.w;
    }
    if (shouldSpawn) {
        uint seed = particleId * 1664525u + simulationStep * 1013904223u;
        float angle = 6.2831853 * randomValue(seed);
        float spread = sqrt(randomValue(seed + 1u));
        vec3 rightVector = cross(emitterForward, vec3(0, 1, 0));
        vec3 right = normalize(rightVector);
        vec3 up = cross(right, emitterForward);
        vec3 radial = cos(angle) * right + sin(angle) * up;
        float horizontalSpread;
        float verticalSpread;
        float speed;
        float velocitySpread;
        float spawnRadius;
        if (stream == 0) {
            horizontalSpread = 0.0;
            verticalSpread = 0.0;
            speed = mix(2.2, 3.2, randomValue(seed + 2u));
            velocitySpread = 0.95;
            spawnRadius = 0.018;
        } else {
            if (stream == 1) {
                horizontalSpread = -0.38;
            } else {
                horizontalSpread = 0.38;
            }
            verticalSpread = 0.10;
            speed = mix(1.7, 2.3, randomValue(seed + 2u));
            velocitySpread = 0.16;
            spawnRadius = 0.006;
        }
        vec3 forwardWithSpread = emitterForward + right * horizontalSpread;
        vec3 direction = normalize(forwardWithSpread + up * verticalSpread);
        vec3 velocity = direction * speed + radial * spread * velocitySpread;
        float lifetime = mix(0.65, 1.05, randomValue(seed + 3u));
        velocityLifetimeState = vec4(velocity, lifetime);

        if (previousLifetime > 0.0) {
            age = min(age - previousLifetime, timestep);
        } else {
            age = min(age, timestep);
        }
        vec3 spawnOffset = radial * spread * spawnRadius;
        positionAgeState.xyz = emitterPosition + spawnOffset;
    }
    if (velocityLifetimeState.w > 0.0) {
        float integrationTime;
        if (shouldSpawn) {
            integrationTime = age;
        } else {
            integrationTime = timestep;
        }
        float horizontalPhase = float(particleId) * 1.7 + age * 8.0;
        float depthPhase = float(particleId) * 2.3 + age * 7.0;
        vec3 acceleration = vec3(0.1 * sin(horizontalPhase), 0.10, 0.1 * cos(depthPhase));
        vec3 velocityDisplacement = velocityLifetimeState.xyz * integrationTime;
        vec3 accelerationDisplacement = 0.5 * acceleration * integrationTime * integrationTime;
        positionAgeState.xyz += velocityDisplacement + accelerationDisplacement;
        velocityLifetimeState.xyz += acceleration * integrationTime;
    }
    nextPositionAge = vec4(positionAgeState.xyz, age);
    nextVelocityLifetime = velocityLifetimeState;
}
